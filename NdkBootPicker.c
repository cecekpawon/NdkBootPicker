                                                                                 //
//  NdkBootPicker.c
//
//
//  Created by N-D-K on 1/24/20.
//

#include <NdkBootPicker.h>
#include <FontData.h>

STATIC
BOOLEAN
mAllowSetDefault = FALSE;

STATIC
UINTN
mDefaultEntry = 0;

STATIC
BOOLEAN
mHideAuxiliary = FALSE;

STATIC
INTN
mCurrentSelection = 0;

STATIC
INTN
mMenuIconsCount = 0;

/*========== Pointer Setting ==========*/

STATIC
POINTERS
mPointer = {
  NULL, NULL, NULL, NULL, NULL,
  { 0, 0, POINTER_WIDTH, POINTER_HEIGHT},
  { 0, 0, POINTER_WIDTH, POINTER_HEIGHT},
  FALSE,
  0,
  { 0, 0, 0, FALSE, FALSE},
  NoEvents
};

STATIC
UINT32
mPointerSpeed = 0;

STATIC
UINT64
mDoubleClickTime = 500;

STATIC
BOOLEAN
mPointerIsActive = FALSE;

/*========== Graphic UI Setting ==========*/

STATIC
EFI_GRAPHICS_OUTPUT_PROTOCOL *
mGraphicsOutput = NULL;

STATIC
EFI_UGA_DRAW_PROTOCOL *
mUgaDraw = NULL;

STATIC
INTN
mScreenWidth = 0;

STATIC
INTN
mScreenHeight = 0;

STATIC
INTN
mFontWidth = 8;

STATIC
INTN
mFontHeight = 18;

STATIC
INTN
mTextHeight = 19;

//
// not actual scale, will be set after getting screen resolution.
// (16 will be no scaling, 28 will be for 4k screen)
//
STATIC
INTN
mTextScale = 0;

STATIC
INTN
mUiScale = 0;

//
// Default 144/288 pixels space to contain icons with size 128x128/256x256
//
STATIC
UINTN
mIconSpaceSize = 0;

STATIC
UINTN
mIconPaddingSize = 0;

STATIC
OC_STORAGE_CONTEXT *
mStorage = NULL;

STATIC
NDK_UI_IMAGE *
mFontImage = NULL;

STATIC
BOOLEAN
mProportional = TRUE;

STATIC
BOOLEAN
mDarkMode = TRUE;

STATIC
NDK_UI_IMAGE *
mBackgroundImage = NULL;

STATIC
NDK_UI_IMAGE *
mMenuImage = NULL;

STATIC
NDK_UI_ICON
mIconReset = { 0, 0, FALSE, NULL, NULL, NULL };

STATIC
NDK_UI_ICON
mIconShutdown = { 0, 0, FALSE, NULL, NULL, NULL };

STATIC
NDK_UI_IMAGE *
mSelectionImage = NULL;

STATIC
NDK_UI_IMAGE *
mLabelImage = NULL;

STATIC
BOOLEAN
mPrintLabel = TRUE;

STATIC
BOOLEAN
mSelectorUsed = TRUE;

/*=========== Default colors settings ==============*/

STATIC
EFI_GRAPHICS_OUTPUT_BLT_PIXEL
mTransparentPixel   = { 0x00, 0x00, 0x00, 0x00 };

STATIC
EFI_GRAPHICS_OUTPUT_BLT_PIXEL
mBluePixel          = { 0x7f, 0x0f, 0x0f, 0xff };

STATIC
EFI_GRAPHICS_OUTPUT_BLT_PIXEL
mBlackPixel         = { 0x00, 0x00, 0x00, 0xff };

STATIC
EFI_GRAPHICS_OUTPUT_BLT_PIXEL
mLowWhitePixel      = { 0xb8, 0xbd, 0xbf, 0xff };

STATIC
EFI_GRAPHICS_OUTPUT_BLT_PIXEL
mGrayPixel          = { 0xaa, 0xaa, 0xaa, 0xff };

// Selection and Entry's description font color
STATIC
EFI_GRAPHICS_OUTPUT_BLT_PIXEL *
mFontColorPixel = &mLowWhitePixel;

// Background color
STATIC
EFI_GRAPHICS_OUTPUT_BLT_PIXEL *
mBackgroundPixel = &mBlackPixel;

/*=========== Functions ==============*/

STATIC
VOID
SystemReset (
  IN EFI_RESET_TYPE   ResetType
  )
{
  if (ResetType == EfiResetCold) {
    DirectResetCold ();
  }
  gRT->ResetSystem (ResetType, EFI_SUCCESS, 0, NULL);
}

STATIC
BOOLEAN
FileExist (
  IN CONST CHAR16   *FilePath
  )
{
  ASSERT (FilePath != NULL);
  ASSERT (StrLen (FilePath) > 0);

  return OcStorageExistsFileUnicode (mStorage, FilePath);
}

STATIC
NDK_UI_IMAGE *
DecodePNGFile (
  IN CONST CHAR16   *FilePath
  )
{
  NDK_UI_IMAGE *  Image;
  UINT8           *FileData;
  UINT32          FileSize;

  ASSERT (FilePath != NULL);
  ASSERT (StrLen (FilePath) > 0);

  Image = NULL;

  FileSize = 0;
  FileData = OcStorageReadFileUnicode (mStorage, FilePath, &FileSize);
  if (FileData != NULL && FileSize > 0) {
    Image = DecodePNG (FileData, FileSize);
  } else {
    DEBUG ((DEBUG_ERROR, "OCUI: Failed to locate %s file\n", FilePath));
  }

  return Image;
}

//STATIC
VOID
DrawImageArea (
  IN NDK_UI_IMAGE   *Image,
  IN INTN           AreaXpos,
  IN INTN           AreaYpos,
  IN INTN           AreaWidth,
  IN INTN           AreaHeight,
  IN INTN           ScreenXpos,
  IN INTN           ScreenYpos
  )
{
  EFI_STATUS    Status;

  if (Image == NULL) {
    return;
  }

  if (ScreenXpos < 0 || ScreenXpos >= mScreenWidth || ScreenYpos < 0 || ScreenYpos >= mScreenHeight) {
    DEBUG ((DEBUG_INFO, "OCUI: Invalid Screen coordinate requested...x:%d - y:%d \n", ScreenXpos, ScreenYpos));
    return;
  }

  if (AreaWidth == 0) {
    AreaWidth   = Image->Width;
  }

  if (AreaHeight == 0) {
    AreaHeight  = Image->Height;
  }

  if ((AreaXpos != 0) || (AreaYpos != 0)) {
    RestrictImageArea (Image, AreaXpos, AreaYpos, &AreaWidth, &AreaHeight);
    if (AreaWidth == 0) {
      DEBUG ((DEBUG_INFO, "OCUI: invalid area position requested\n"));
      return;
    }
  }

  if (ScreenXpos + AreaWidth > mScreenWidth) {
    AreaWidth   = mScreenWidth - ScreenXpos;
  }

  if (ScreenYpos + AreaHeight > mScreenHeight) {
    AreaHeight  = mScreenHeight - ScreenYpos;
  }

  if (mGraphicsOutput != NULL) {
    Status = mGraphicsOutput->Blt(mGraphicsOutput,
                                  Image->Bitmap,
                                  EfiBltBufferToVideo,
                                  (UINTN) AreaXpos,
                                  (UINTN) AreaYpos,
                                  (UINTN) ScreenXpos,
                                  (UINTN) ScreenYpos,
                                  (UINTN) AreaWidth,
                                  (UINTN) AreaHeight,
                                  (UINTN) Image->Width * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL)
                                  );
  } else {
    ASSERT (mUgaDraw != NULL);
    Status = mUgaDraw->Blt(mUgaDraw,
                            (EFI_UGA_PIXEL *) Image->Bitmap,
                            EfiUgaBltBufferToVideo,
                            (UINTN) AreaXpos,
                            (UINTN) AreaYpos,
                            (UINTN) ScreenXpos,
                            (UINTN) ScreenYpos,
                            (UINTN) AreaWidth,
                            (UINTN) AreaHeight,
                            (UINTN) Image->Width * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL)
                            );
  }

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "OCUI: Draw Image Area...%r\n", Status));
  }
}

STATIC
VOID
TakeImage (
  IN NDK_UI_IMAGE      *Image,
  IN INTN              ScreenXpos,
  IN INTN              ScreenYpos,
  IN INTN              AreaWidth,
  IN INTN              AreaHeight
  )
{
  //EFI_STATUS    Status;

  if (ScreenXpos + AreaWidth > mScreenWidth) {
    AreaWidth   = mScreenWidth - ScreenXpos;
  }

  if (ScreenYpos + AreaHeight > mScreenHeight) {
    AreaHeight  = mScreenHeight - ScreenYpos;
  }

  if (mGraphicsOutput != NULL) {
    /*Status =*/ mGraphicsOutput->Blt(mGraphicsOutput,
                                     (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *) Image->Bitmap,
                                     EfiBltVideoToBltBuffer,
                                     ScreenXpos,
                                     ScreenYpos,
                                     0,
                                     0,
                                     AreaWidth,
                                     AreaHeight,
                                     (UINTN) Image->Width * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL)
                                     );
  } else {
    ASSERT (mUgaDraw != NULL);

    /*Status =*/ mUgaDraw->Blt(mUgaDraw,
                              (EFI_UGA_PIXEL *) Image->Bitmap,
                              EfiUgaVideoToBltBuffer,
                              ScreenXpos,
                              ScreenYpos,
                              0,
                              0,
                              AreaWidth,
                              AreaHeight,
                              (UINTN) Image->Width * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL)
                              );
  }
}

STATIC
VOID
TakeScreenShot (
  IN CHAR16   *FilePath
  )
{
  EFI_STATUS              Status;
  EFI_FILE_PROTOCOL       *Fs;
  EFI_TIME                Date;
  NDK_UI_IMAGE            *Image;
  EFI_UGA_PIXEL           *ImagePNG;
  VOID                    *Buffer;
  UINTN                   BufferSize;
  UINTN                   Index;
  UINTN                   ImageSize;
  CHAR16                  *Path;
  UINTN                   Size;

  Buffer     = NULL;
  BufferSize = 0;

  Status = gRT->GetTime (&Date, NULL);
  if (EFI_ERROR (Status)) {
    ZeroMem (&Date, sizeof (Date));
  }

  Size = StrSize (FilePath) + L_STR_SIZE (L"-0000-00-00-000000.png");
  Path = AllocatePool (Size);
  if (Path == NULL) {
    return;
  }
  UnicodeSPrint (Path,
                 Size,
                 L"%s-%04u-%02u-%02u-%02u%02u%02u.png",
                 FilePath,
                 (UINT32) Date.Year,
                 (UINT32) Date.Month,
                 (UINT32) Date.Day,
                 (UINT32) Date.Hour,
                 (UINT32) Date.Minute,
                 (UINT32) Date.Second
  );

  Image = CreateImage ((UINT16) mScreenWidth, (UINT16) mScreenHeight, FALSE);
  if (Image == NULL) {
    DEBUG ((DEBUG_INFO, "Failed to take screen shot!\n"));
    return;
  }

  TakeImage (Image, 0, 0, mScreenWidth, mScreenHeight);

  ImagePNG  = (EFI_UGA_PIXEL *) Image->Bitmap;
  ImageSize = Image->Width * Image->Height;

  // Convert BGR to RGBA with Alpha set to 0xFF
  for (Index = 0; Index < ImageSize; ++Index) {
    UINT8 Temp = ImagePNG[Index].Blue;
    ImagePNG[Index].Blue      = ImagePNG[Index].Red;
    ImagePNG[Index].Red       = Temp;
    ImagePNG[Index].Reserved  = 0xFF;
  }

  // Encode raw RGB image to PNG format
  Status = OcEncodePng (ImagePNG,
                      (UINTN) Image->Width,
                      (UINTN) Image->Height,
                      &Buffer,
                      &BufferSize
                      );

  FreeImage (Image);

  if (Buffer == NULL) {
    DEBUG ((DEBUG_INFO, "OCUI: Fail Encoding!\n"));
    return;
  }

  Status = mStorage->FileSystem->OpenVolume (mStorage->FileSystem, &Fs);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "OCUI: Locating Writeable file system - %r\n", Status));
    return;
  }

  Status = SetFileData (Fs, Path, Buffer, (UINT32) BufferSize);
  DEBUG ((DEBUG_INFO, "OCUI: Screenshot was taken - %r\n", Status));

  if (Buffer != NULL) {
    FreePool (Buffer);
  }

  if (Path != NULL) {
    FreePool (Path);
  }
}

/* Mouse Functions Begin */

STATIC
VOID
HidePointer (
  VOID
  )
{
  if (mPointer.SimplePointerProtocol != NULL && mPointerIsActive) {
    DrawImageArea (mPointer.OldImage, 0, 0, 0, 0, mPointer.OldPlace.Xpos, mPointer.OldPlace.Ypos);
  }
}

STATIC
VOID
DrawPointer (
  VOID
  )
{
  if (mPointer.SimplePointerProtocol == NULL || !mPointerIsActive) {
    return;
  }

  TakeImage (mPointer.OldImage,
             mPointer.NewPlace.Xpos,
             mPointer.NewPlace.Ypos,
             POINTER_WIDTH,
             POINTER_HEIGHT
             );

  CopyMem (&mPointer.OldPlace, &mPointer.NewPlace, sizeof(AREA_RECT));

  CopyMem (mPointer.NewImage->Bitmap,
           mPointer.OldImage->Bitmap,
           (UINTN) (POINTER_WIDTH * POINTER_HEIGHT * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL))
           );

  RawCompose (mPointer.NewImage->Bitmap,
              mPointer.IsClickable ? mPointer.PointerAlt->Bitmap : mPointer.Pointer->Bitmap,
              mPointer.NewImage->Width,
              mPointer.NewImage->Height,
              mPointer.NewImage->Width,
              mPointer.Pointer->Width
              );

  DrawImageArea (mPointer.NewImage,
                 0,
                 0,
                 POINTER_WIDTH,
                 POINTER_HEIGHT,
                 mPointer.OldPlace.Xpos,
                 mPointer.OldPlace.Ypos
                 );
}

STATIC
VOID
RedrawPointer (
  VOID
  )
{
  if (mPointer.SimplePointerProtocol == NULL) {
   return;
  }

  HidePointer ();
  DrawPointer ();
}

STATIC
EFI_STATUS
InitMouse (
  VOID
  )
{
  EFI_STATUS          Status;
  CONST CHAR16        *FilePath;
  UINTN               DataSize;

  if (mPointer.SimplePointerProtocol != NULL) {
    DrawPointer ();
    return EFI_SUCCESS;
  }

  Status = OcHandleProtocolFallback (
    gST->ConsoleInHandle,
    &gEfiSimplePointerProtocolGuid,
    (VOID **) &mPointer.SimplePointerProtocol
    );

  if (EFI_ERROR (Status)) {
    mPointer.Pointer = NULL;
    mPointer.MouseEvent = NoEvents;
    mPointer.SimplePointerProtocol = NULL;
    DEBUG ((DEBUG_INFO, "OCUI: No Mouse found!\n"));
    return Status;
  }

  if (mUiScale == 28 || mScreenHeight >= 2160) {
    FilePath = UI_IMAGE_POINTER;
  } else {
    FilePath = UI_IMAGE_POINTER_ALT;
  }

  if (FileExist (FilePath)) {
    mPointer.Pointer = DecodePNGFile (FilePath);
  } else {
    mPointer.Pointer = CreateFilledImage (POINTER_WIDTH, POINTER_HEIGHT, TRUE, &mBluePixel);
  }

  if (FileExist (UI_IMAGE_POINTER_HAND)) {
    mPointer.PointerAlt = DecodePNGFile (UI_IMAGE_POINTER_HAND);
  } else {
    mPointer.PointerAlt = CreateFilledImage (POINTER_WIDTH, POINTER_HEIGHT, TRUE, &mBluePixel);
  }

  if (mPointer.Pointer == NULL) {
    DEBUG ((DEBUG_INFO, "OCUI: No Mouse Icon found!\n"));
    mPointer.SimplePointerProtocol = NULL;
    return EFI_NOT_FOUND;
  }

  mPointer.LastClickTime    = 0;
  mPointer.OldPlace.Xpos    = (INTN) (mScreenWidth >> 2);
  mPointer.OldPlace.Ypos    = (INTN) (mScreenHeight >> 2);
  mPointer.OldPlace.Width   = POINTER_WIDTH;
  mPointer.OldPlace.Height  = POINTER_HEIGHT;

  CopyMem (&mPointer.NewPlace, &mPointer.OldPlace, sizeof (AREA_RECT));

  mPointer.OldImage = CreateImage (POINTER_WIDTH, POINTER_HEIGHT, FALSE);
  mPointer.NewImage = CreateFilledImage (POINTER_WIDTH, POINTER_HEIGHT, TRUE, &mTransparentPixel);

  DataSize = sizeof (mPointerSpeed);

  Status = gRT->GetVariable (UI_MENU_POINTER_SPEED,
                             &gAppleVendorVariableGuid,
                             NULL,
                             &DataSize,
                             &mPointerSpeed
                             );

  if (EFI_ERROR (Status) || mPointerSpeed == 0) {
    //DEBUG ((DEBUG_INFO, "OCUI: No PointerSpeed setting found! - %r\n", Status));
    mPointerSpeed = 2; //6
  } else {
    //DEBUG ((DEBUG_INFO, "OCUI: Set PointerSpeed to %d\n", mPointerSpeed));
  }

  return Status;
}

STATIC
VOID
PointerUpdate (
  VOID
  )
{
  UINT64                      Now;
  EFI_STATUS                  Status;
  EFI_SIMPLE_POINTER_STATE    tmpState;
  EFI_SIMPLE_POINTER_MODE     *CurrentMode;
  INTN                        ScreenRelX;
  INTN                        ScreenRelY;

  Now = GetTimeInNanoSecond (GetPerformanceCounter ());

  Status = mPointer.SimplePointerProtocol->GetState (mPointer.SimplePointerProtocol, &tmpState);
  if (!EFI_ERROR (Status)) {
    if (!mPointer.State.LeftButton && tmpState.LeftButton) {
      mPointer.MouseEvent = LeftMouseDown;
    } else if (!mPointer.State.RightButton && tmpState.RightButton) {
      mPointer.MouseEvent = RightMouseDown;
    } else if (mPointer.State.LeftButton && !tmpState.LeftButton) {
      if (Now < (mPointer.LastClickTime + mDoubleClickTime * 1000000ULL)) {
        mPointer.MouseEvent = DoubleClick;
      } else {
        mPointer.MouseEvent = LeftClick;
      }
      mPointer.LastClickTime = Now;
    } else if (mPointer.State.RightButton && !tmpState.RightButton) {
      mPointer.MouseEvent = RightClick;
    } else if (mPointer.State.RelativeMovementZ > 0) {
      mPointer.MouseEvent = ScrollDown;
    } else if (mPointer.State.RelativeMovementZ < 0) {
      mPointer.MouseEvent = ScrollUp;
    } else if (mPointer.State.RelativeMovementX || mPointer.State.RelativeMovementY) {
      if (!mPointerIsActive) {
        mPointerIsActive = TRUE;
        DrawPointer ();
      }
      mPointer.MouseEvent = MouseMove;
    } else {
      mPointer.MouseEvent = NoEvents;
    }

    CopyMem (&mPointer.State, &tmpState, sizeof(EFI_SIMPLE_POINTER_STATE));

    CurrentMode = mPointer.SimplePointerProtocol->Mode;
    ScreenRelX  = (mScreenWidth * mPointer.State.RelativeMovementX * (INTN) mPointerSpeed / (INTN) CurrentMode->ResolutionX) >> 10;

    mPointer.NewPlace.Xpos += ScreenRelX;

    if (mPointer.NewPlace.Xpos < 0) {
      mPointer.NewPlace.Xpos = 0;
    }

    if (mPointer.NewPlace.Xpos > mScreenWidth - 1) {
      mPointer.NewPlace.Xpos = mScreenWidth - 1;
    }

    ScreenRelY = (mScreenHeight * mPointer.State.RelativeMovementY * (INTN) mPointerSpeed / (INTN) CurrentMode->ResolutionY) >> 10;
    mPointer.NewPlace.Ypos += ScreenRelY;

    if (mPointer.NewPlace.Ypos < 0) {
      mPointer.NewPlace.Ypos = 0;
    }

    if (mPointer.NewPlace.Ypos > mScreenHeight - 1) {
      mPointer.NewPlace.Ypos = mScreenHeight - 1;
    }

    RedrawPointer();
  }
}

BOOLEAN
MouseInRect (
  IN AREA_RECT    *Place
  )
{
  return  ((mPointer.NewPlace.Xpos >= Place->Xpos)
           && (mPointer.NewPlace.Xpos < (Place->Xpos + (INTN) Place->Width))
           && (mPointer.NewPlace.Ypos >= Place->Ypos)
           && (mPointer.NewPlace.Ypos < (Place->Ypos + (INTN) Place->Height))
           );
}

BOOLEAN
IsMouseInPlace (
  IN INTN   Xpos,
  IN INTN   Ypos,
  IN INTN   AreaWidth,
  IN INTN   AreaHeight
  )
{
  AREA_RECT   Place;

  Place.Xpos    = Xpos;
  Place.Ypos    = Ypos;
  Place.Width   = AreaWidth;
  Place.Height  = AreaHeight;

  return  MouseInRect (&Place);
}

STATIC
VOID
CreateMenuImage (
  IN NDK_UI_IMAGE   *Icon,
  IN UINTN          IconCount
  )
{
  NDK_UI_IMAGE    *NewImage;
  UINT16          Width;
  UINT16          Height;
  BOOLEAN         IsTwoRow;
  UINTN           IconsPerRow;
  INTN            Xpos;
  INTN            Ypos;
  INTN            Offset;
  INTN            IconRowSpace;

  NewImage      = NULL;
  Xpos          = 0;
  Ypos          = 0;
  IconRowSpace  = (32 * mUiScale >> 4) + ICON_ROW_SPACE_OFFSET;

  if (mMenuImage != NULL) {
    Width     = mMenuImage->Width;
    Height    = mMenuImage->Height;
    IsTwoRow  = mMenuImage->Height > mIconSpaceSize + IconRowSpace;

    if (IsTwoRow) {
      IconsPerRow   = mMenuImage->Width / mIconSpaceSize;
      Xpos          = (IconCount - IconsPerRow) * mIconSpaceSize;
      Ypos          = mIconSpaceSize + IconRowSpace;
    } else {
      if (mMenuImage->Width + (mIconSpaceSize * 2) <= (UINT16) mScreenWidth) {
        Width   = (UINT16) (mMenuImage->Width + mIconSpaceSize);
        Xpos    = mMenuImage->Width;
      } else {
        Height  = (UINT16) (mMenuImage->Height + mIconSpaceSize + IconRowSpace);
        Ypos    = mIconSpaceSize + IconRowSpace;
      }
    }
  } else {
    Width   = (UINT16) mIconSpaceSize;
    Height  = Width;
  }

  NewImage = CreateFilledImage (Width, Height, TRUE, &mTransparentPixel);
  if (NewImage == NULL) {
    return;
  }

  if (mMenuImage != NULL) {
    ComposeImage (NewImage, mMenuImage, 0, 0);
    if (mMenuImage != NULL) {
      FreeImage (mMenuImage);
    }
  }

  Offset = (mIconSpaceSize - (Icon->Width + (mIconPaddingSize * 2))) > 0 ? (mIconSpaceSize - (Icon->Width + (mIconPaddingSize * 2))) / 2 : 0;

  ComposeImage (NewImage, Icon, Xpos + mIconPaddingSize + Offset, Ypos + mIconPaddingSize + Offset);

  if (Icon != NULL) {
    FreeImage (Icon);
  }

  mMenuImage = NewImage;
}

STATIC
VOID
BltImageAlpha (
  IN NDK_UI_IMAGE                     *Image,
  IN INTN                             Xpos,
  IN INTN                             Ypos,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL    *BackgroundPixel,
  IN INTN                             Scale
  )
{
  NDK_UI_IMAGE    *CompImage;
  NDK_UI_IMAGE    *NewImage;
  INTN            Width;
  INTN            Height;

  NewImage  = NULL;
  Width     = Scale << 3;
  Height    = Width;

  if (Image != NULL) {
    NewImage  = CopyScaledImage (Image, Scale);
    Width     = NewImage->Width;
    Height    = NewImage->Height;
  }

  CompImage = CreateFilledImage (Width, Height, (mBackgroundImage != NULL), BackgroundPixel);
  ComposeImage (CompImage, NewImage, 0, 0);

  if (NewImage != NULL) {
    FreeImage (NewImage);
  }

  if (mBackgroundImage == NULL) {
    DrawImageArea (CompImage, 0, 0, 0, 0, Xpos, Ypos);
    FreeImage (CompImage);
    return;
  }

  // Background Image was used.
  NewImage = CreateImage ((UINT16) Width, (UINT16) Height, FALSE);
  if (NewImage == NULL) {
    return;
  }

  RawCopy (NewImage->Bitmap,
           mBackgroundImage->Bitmap + Ypos * mBackgroundImage->Width + Xpos,
           Width,
           Height,
           Width,
           mBackgroundImage->Width
           );

  // Compose
  ComposeImage (NewImage, CompImage, 0, 0);
  FreeImage (CompImage);

  // Draw to screen
  DrawImageArea (NewImage, 0, 0, 0, 0, Xpos, Ypos);
  FreeImage (NewImage);
}

STATIC
VOID
BltMenuImage (
  IN NDK_UI_IMAGE   *Image,
  IN INTN           Xpos,
  IN INTN           Ypos
  )
{
  NDK_UI_IMAGE    *NewImage;

  if (Image == NULL) {
    return;
  }

  NewImage = CreateImage (Image->Width, Image->Height, FALSE);
  if (NewImage == NULL) {
    return;
  }

  RawCopy (NewImage->Bitmap,
           mBackgroundImage->Bitmap + Ypos * mBackgroundImage->Width + Xpos,
           Image->Width,
           Image->Height,
           Image->Width,
           mBackgroundImage->Width
           );

  RawComposeColor (NewImage->Bitmap,
                   Image->Bitmap,
                   NewImage->Width,
                   NewImage->Height,
                   NewImage->Width,
                   Image->Width,
                   ICON_BRIGHTNESS_FULL
                   );

  DrawImageArea (NewImage, 0, 0, 0, 0, Xpos, Ypos);

  FreeImage (NewImage);
}

STATIC
VOID
CreateIcon (
  IN CHAR16               *Name,
  IN OC_BOOT_ENTRY_TYPE   Type,
  IN UINTN                IconCount,
  IN BOOLEAN              Ext,
  IN BOOLEAN              Dmg
  )
{
  CONST CHAR16    *FilePath;
  NDK_UI_IMAGE    *Icon;
  NDK_UI_IMAGE    *ScaledImage;
  INTN            IconScale;

  Icon          = NULL;
  ScaledImage   = NULL;
  IconScale     = 16;

  switch (Type) {
    case OC_BOOT_WINDOWS:
      if (StrStr (Name, L"10") != NULL) {
        FilePath = UI_ICON_WIN10;
      } else {
        FilePath = UI_ICON_WIN;
      }
      break;
    case OC_BOOT_APPLE_OS:
      if (StrStr (Name, L"Install") != NULL) {
        FilePath = UI_ICON_INSTALL;
      } else if (StrStr (Name, L"Cata") != NULL) {
        FilePath = UI_ICON_MAC_CATA;
      } else if (StrStr (Name, L"Moja") != NULL) {
        FilePath = UI_ICON_MAC_MOJA;
      } else if (StrStr (Name, L"Clone") != NULL) {
        FilePath = UI_ICON_CLONE;
      } else {
        FilePath = UI_ICON_MAC;
      }
      break;
    case OC_BOOT_APPLE_RECOVERY:
      FilePath = UI_ICON_MAC_RECOVERY;
      break;
    case OC_BOOT_APPLE_TIME_MACHINE:
      FilePath = UI_ICON_CLONE;
      break;
    case OC_BOOT_EXTERNAL_OS:
      if (StrStr (Name, L"Free") != NULL) {
        FilePath = UI_ICON_FREEBSD;
      } else if (StrStr (Name, L"Arch") != NULL) {
        FilePath = UI_ICON_ARCH;
      } else if (StrStr (Name, L"Debian") != NULL) {
        FilePath = UI_ICON_DEBIAN;
      } else if (StrStr (Name, L"Fedora") != NULL) {
        FilePath = UI_ICON_FEDORA;
      } else if (StrStr (Name, L"Linux") != NULL) {
        FilePath = UI_ICON_LINUX;
      } else if (StrStr (Name, L"Redhat") != NULL) {
        FilePath = UI_ICON_REDHAT;
      } else if (StrStr (Name, L"Ubuntu") != NULL) {
        FilePath = UI_ICON_UBUNTU;
      } else if (StrStr (Name, L"10") != NULL) {
        FilePath = UI_ICON_WIN10;
      } else if (StrStr (Name, L"Win") != NULL) {
        FilePath = UI_ICON_WIN;
      } else {
        FilePath = UI_ICON_CUSTOM;
      }
      break;
    case OC_BOOT_EXTERNAL_TOOL:
      if (StrStr (Name, L"Shell") != NULL) {
        FilePath = UI_ICON_SHELL;
      } else {
          FilePath = UI_ICON_CUSTOM;
      }
      break;
    case OC_BOOT_APPLE_ANY:
      FilePath = UI_ICON_MAC;
      break;
    case OC_BOOT_RESET_NVRAM:
      FilePath = UI_ICON_RESETNVRAM;
      break;
    case OC_BOOT_UNKNOWN:
      FilePath = UI_ICON_UNKNOWN;
      break;

    default:
      FilePath = UI_ICON_UNKNOWN;
      break;
  }

  if (FileExist (FilePath)) {
    Icon = DecodePNGFile (FilePath);
  } else {
    Icon = CreateFilledImage ((mIconSpaceSize - (mIconPaddingSize * 2)), (mIconSpaceSize - (mIconPaddingSize * 2)), TRUE, &mBluePixel);
  }

  if (Icon->Width == 256 && mScreenHeight < 2160) {
    IconScale = 8;
  }

  if (Icon->Width == 256 && mScreenHeight <= 800) {
    IconScale = 4;
  }

  if (Icon->Width > 128 && mMenuImage == NULL) {
    mIconSpaceSize  = ((Icon->Width * IconScale) >> 4) + (mIconPaddingSize * 2);
    mUiScale        = (mUiScale == 8) ? 8 : 16;
  }

  ScaledImage = CopyScaledImage (Icon, (IconScale < mUiScale) ? IconScale : mUiScale);

  FreeImage (Icon);

  CreateMenuImage (ScaledImage, IconCount);
}

STATIC
VOID
SwitchIconSelection (
  IN UINTN      IconCount,
  IN UINTN      IconIndex,
  IN BOOLEAN    Selected,
  IN BOOLEAN    Clicked
  )
{
  NDK_UI_IMAGE    *NewImage;
  NDK_UI_IMAGE    *SelectorImage;
  NDK_UI_IMAGE    *Icon;
  //BOOLEAN         IsTwoRow;
  INTN            Xpos;
  INTN            Ypos;
  INTN            Offset;
  UINT16          Width;
  //UINT16          Height;
  UINTN           IconsPerRow;
  INTN            IconRowSpace;
  INTN            AnimatedDistance;

  /* Begin Calculating Xpos and Ypos of current selected icon on screen*/
  NewImage          = NULL;
  Icon              = NULL;
  //IsTwoRow          = FALSE;
  Xpos              = 0;
  Ypos              = 0;
  Width             = (UINT16) mIconSpaceSize;
  //Height            = Width;
  IconsPerRow       = 1;
  IconRowSpace      = (32 * mUiScale >> 4) + ICON_ROW_SPACE_OFFSET;
  AnimatedDistance  = (mIconPaddingSize + 1) >> 1;

  for (IconsPerRow = 1; IconsPerRow < IconCount; ++IconsPerRow) {
    Width = Width + (UINT16) mIconSpaceSize;
    if ((Width + (mIconSpaceSize * 2)) >= (UINT16) mScreenWidth) {
      break;
    }
  }

  if (IconsPerRow < IconCount) {
    //IsTwoRow = TRUE;
    //Height = (UINT16) mIconSpaceSize * 2;
    if (IconIndex <= IconsPerRow) {
      Xpos = (mScreenWidth - Width) / 2 + (mIconSpaceSize * IconIndex);
      Ypos = (mScreenHeight / 2) - mIconSpaceSize;
    } else {
      Xpos = (mScreenWidth - Width) / 2 + (mIconSpaceSize * (IconIndex - (IconsPerRow + 1)));
      Ypos = mScreenHeight / 2 + IconRowSpace;
    }
  } else {
    Xpos = (mScreenWidth - Width) / 2 + (mIconSpaceSize * IconIndex);
    Ypos = (mScreenHeight / 2) - mIconSpaceSize;
  }
  /* Done Calculating Xpos and Ypos of current selected icon on screen*/

  Icon = CreateImage ((UINT16) (mIconSpaceSize - (mIconPaddingSize * 2)), (UINT16) (mIconSpaceSize - (mIconPaddingSize * 2)), TRUE);
  if (Icon == NULL) {
    return;
  }

  RawCopy (Icon->Bitmap,
           mMenuImage->Bitmap
           + ((IconIndex <= IconsPerRow)
              ? mIconPaddingSize
              : mIconPaddingSize + mIconSpaceSize + IconRowSpace) * mMenuImage->Width + ((Xpos + mIconPaddingSize) - ((mScreenWidth - Width) / 2)),
           Icon->Width,
           Icon->Height,
           Icon->Width,
           mMenuImage->Width
           );

  if (Selected && mSelectorUsed) {
    NewImage = CreateImage ((UINT16) mIconSpaceSize, (UINT16) (mIconSpaceSize + AnimatedDistance), FALSE);

    RawCopy (NewImage->Bitmap,
             mBackgroundImage->Bitmap + (Ypos - AnimatedDistance) * mBackgroundImage->Width + Xpos,
             mIconSpaceSize,
             mIconSpaceSize + AnimatedDistance,
             mIconSpaceSize,
             mBackgroundImage->Width
             );

    SelectorImage = CopyScaledImage (mSelectionImage, (mSelectionImage->Width == mIconSpaceSize) ? 16 : mUiScale);

    Offset = (NewImage->Width - SelectorImage->Width) >> 1;

    RawCompose (NewImage->Bitmap + (Clicked ? Offset + AnimatedDistance : Offset) * NewImage->Width + Offset,
                SelectorImage->Bitmap,
                SelectorImage->Width,
                SelectorImage->Height,
                NewImage->Width,
                SelectorImage->Width
                );

    FreeImage (SelectorImage);
  } else {
    NewImage = CreateImage ((UINT16) mIconSpaceSize, (UINT16) (mIconSpaceSize + AnimatedDistance), FALSE);

    RawCopy (NewImage->Bitmap,
             mBackgroundImage->Bitmap + (Ypos - AnimatedDistance) * mBackgroundImage->Width + Xpos,
             mIconSpaceSize,
             mIconSpaceSize + AnimatedDistance,
             mIconSpaceSize,
             mBackgroundImage->Width
             );
  }

  RawComposeColor (NewImage->Bitmap
                      + ((Selected && !Clicked)
                        ? mIconPaddingSize
                        : mIconPaddingSize + AnimatedDistance) * NewImage->Width + mIconPaddingSize,
                   Icon->Bitmap,
                   Icon->Width,
                   Icon->Height,
                   NewImage->Width,
                   Icon->Width,
                   !Selected ? ICON_BRIGHTNESS_FULL : ICON_BRIGHTNESS_LEVEL
                   );

  FreeImage (Icon);

  BltImage (NewImage, Xpos, Ypos - AnimatedDistance);

  FreeImage (NewImage);
}

STATIC
VOID
ScaleBackgroundImage (
  VOID
  )
{
  NDK_UI_IMAGE                    *Image;
  INTN                            OffsetX;
  INTN                            OffsetX1;
  INTN                            OffsetX2;
  INTN                            OffsetY;
  INTN                            OffsetY1;
  INTN                            OffsetY2;
  UINTN                           Index;
  UINTN                           Index1;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL   *Pixel;

  // Tile //
  if (mBackgroundImage->Width < (mScreenWidth / 4)) {
    Image = CopyImage (mBackgroundImage);
    FreeImage (mBackgroundImage);
    mBackgroundImage = CreateFilledImage (mScreenWidth, mScreenHeight, FALSE, &mGrayPixel);

    OffsetX = (Image->Width * ((mScreenWidth - 1) / Image->Width + 1) - mScreenWidth) >> 1;
    OffsetY = (Image->Height * ((mScreenHeight - 1) / Image->Height + 1) - mScreenHeight) >> 1;

    Pixel = mBackgroundImage->Bitmap;
    for (Index = 0; Index < (UINTN) mScreenHeight; Index++) {
      OffsetY1 = ((Index + OffsetY) % Image->Height) * Image->Width;
      for (Index1 = 0; Index1 < (UINTN) mScreenWidth; Index1++) {
        *Pixel++ = Image->Bitmap[OffsetY1 + ((Index1 + OffsetX) % Image->Width)];
      }
    }
  // Scale & Crop //
  } else {
    Image = CopyScaledImage (mBackgroundImage, (mScreenWidth << 4) / mBackgroundImage->Width);

    FreeImage (mBackgroundImage);

    mBackgroundImage = CreateFilledImage (mScreenWidth, mScreenHeight, FALSE, &mGrayPixel);

    OffsetX = mScreenWidth - Image->Width;
    if (OffsetX >= 0) {
      OffsetX1  = OffsetX >> 1;
      OffsetX2  = 0;
      OffsetX   = Image->Width;
    } else {
      OffsetX1  = 0;
      OffsetX2  = (-OffsetX) >> 1;
      OffsetX   = mScreenWidth;
    }

    OffsetY = mScreenHeight - Image->Height;
    if (OffsetY >= 0) {
      OffsetY1  = OffsetY >> 1;
      OffsetY2  = 0;
      OffsetY   = Image->Height;
    } else {
      OffsetY1  = 0;
      OffsetY2  = (-OffsetY) >> 1;
      OffsetY   = mScreenHeight;
    }

    RawCopy (mBackgroundImage->Bitmap + OffsetY1 * mScreenWidth + OffsetX1,
             Image->Bitmap + OffsetY2 * Image->Width + OffsetX2,
             OffsetX,
             OffsetY,
             mScreenWidth,
             Image->Width
             );
  }

  FreeImage (Image);
}

STATIC
VOID
ClearScreen (
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL    *Color
  )
{
  NDK_UI_IMAGE    *Image;

  if (FileExist (UI_IMAGE_BACKGROUND) && mScreenHeight >= 2160) {
    mBackgroundImage = DecodePNGFile (UI_IMAGE_BACKGROUND);
  } else if (FileExist (UI_IMAGE_BACKGROUND_ALT)) {
    mBackgroundImage = DecodePNGFile (UI_IMAGE_BACKGROUND_ALT);
  }

  if (mBackgroundImage != NULL && (mBackgroundImage->Width != mScreenWidth || mBackgroundImage->Height != mScreenHeight)) {
    ScaleBackgroundImage ();
  }

  if (mBackgroundImage == NULL) {
    if (FileExist (UI_IMAGE_BACKGROUND_COLOR)) {
      Image = DecodePNGFile (UI_IMAGE_BACKGROUND_COLOR);
      if (Image != NULL) {
        CopyMem (mBackgroundPixel, &Image->Bitmap[0], sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
        mBackgroundPixel->Reserved = 0xff;
      }
      FreeImage (Image);
    }
    mBackgroundImage = CreateFilledImage (mScreenWidth, mScreenHeight, FALSE, mBackgroundPixel);
  }

  if (mBackgroundImage != NULL) {
    BltImage (mBackgroundImage, 0, 0);
  }

  if (FileExist (UI_IMAGE_FONT_COLOR)) {
    Image = DecodePNGFile (UI_IMAGE_FONT_COLOR);
    if (Image != NULL) {
      CopyMem (mFontColorPixel, &Image->Bitmap[0], sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
      mFontColorPixel->Reserved = 0xff;
      FreeImage (Image);
    }
  }

  if (FileExist (UI_IMAGE_SELECTOR_OFF)) {
    mSelectorUsed = FALSE;
  }

  if (mSelectorUsed && FileExist (UI_IMAGE_SELECTOR) && mScreenHeight >= 2160) {
    mSelectionImage = DecodePNGFile (UI_IMAGE_SELECTOR);
  } else if (mSelectorUsed && FileExist (UI_IMAGE_SELECTOR_ALT)) {
    mSelectionImage = DecodePNGFile (UI_IMAGE_SELECTOR_ALT);
  } else {
    mSelectionImage = CreateFilledImage (mIconSpaceSize, mIconSpaceSize, FALSE, mFontColorPixel);
  }

  if (FileExist (UI_IMAGE_LABEL_OFF)) {
    mPrintLabel = FALSE;
  }

  if (FileExist (UI_IMAGE_LABEL)) {
    mLabelImage = DecodePNGFile (UI_IMAGE_LABEL);
  } else {
    mLabelImage = CreateFilledImage (mIconSpaceSize, 32, TRUE, &mTransparentPixel);
  }
}

STATIC
VOID
ClearScreenArea (
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL    *Color,
  IN INTN                             Xpos,
  IN INTN                             Ypos,
  IN INTN                             Width,
  IN INTN                             Height
  )
{
  NDK_UI_IMAGE    *Image;
  NDK_UI_IMAGE    *NewImage;

  Image = CreateFilledImage (Width, Height, (mBackgroundImage != NULL), Color);

  if (mBackgroundImage == NULL) {
    DrawImageArea (Image, 0, 0, 0, 0, Xpos, Ypos);
    FreeImage (Image);
    return;
  }

  NewImage = CreateImage ((UINT16) Width, (UINT16) Height, FALSE);
  if (NewImage == NULL) {
    return;
  }

  RawCopy (NewImage->Bitmap,
           mBackgroundImage->Bitmap + Ypos * mBackgroundImage->Width + Xpos,
           Width,
           Height,
           Width,
           mBackgroundImage->Width
           );

  ComposeImage (NewImage, Image, 0, 0);
  FreeImage (Image);

  DrawImageArea (NewImage, 0, 0, 0, 0, Xpos, Ypos);
  FreeImage (NewImage);
}

STATIC
VOID
InitScreen (
  VOID
  )
{
  EFI_STATUS    Status;
  //EFI_HANDLE    Handle;
  UINT32        ColorDepth;
  UINT32        RefreshRate;
  UINT32        ScreenWidth;
  UINT32        ScreenHeight;

  //Handle    = NULL;
  mUgaDraw  = NULL;

  //
  // Try to open GOP first
  //
  if (mGraphicsOutput == NULL) {
    Status = gBS->HandleProtocol (gST->ConsoleOutHandle, &gEfiGraphicsOutputProtocolGuid, (VOID **) &mGraphicsOutput);
    if (EFI_ERROR (Status)) {
      mGraphicsOutput = NULL;
      //
      // Open GOP failed, try to open UGA
      //
      Status = gBS->HandleProtocol (gST->ConsoleOutHandle, &gEfiUgaDrawProtocolGuid, (VOID **) &mUgaDraw);
      if (EFI_ERROR (Status)) {
        mUgaDraw = NULL;
      }
    }
  }

  if (mGraphicsOutput != NULL) {
    Status = OcSetConsoleResolution (0, 0, 0, FALSE);
    mScreenWidth  = mGraphicsOutput->Mode->Info->HorizontalResolution;
    mScreenHeight = mGraphicsOutput->Mode->Info->VerticalResolution;
  } else {
    ASSERT (mUgaDraw != NULL);
    Status = mUgaDraw->GetMode (mUgaDraw, &ScreenWidth, &ScreenHeight, &ColorDepth, &RefreshRate);
    mScreenWidth  = ScreenWidth;
    mScreenHeight = ScreenHeight;
  }
  DEBUG ((DEBUG_INFO, "OCUI: Initialize Graphic Screen...%r\n", Status));

  mTextScale = (mTextScale == 0 && mScreenHeight >= 2160 && !(FileExist (UI_IMAGE_TEXT_SCALE_OFF))) ? 32 : 16;
  if (mUiScale == 0 && mScreenHeight >= 2160 && !(FileExist (UI_IMAGE_ICON_SCALE_OFF))) {
    mUiScale          = 32;
    mIconPaddingSize  = 16;
    mIconSpaceSize    = 288;
  } else if (mUiScale == 0 && mScreenHeight <= 800) {
    mUiScale          = 8;
    mTextScale        = 16;
    mIconPaddingSize  = 3;
    mIconSpaceSize    = 70;
    mPrintLabel       = FALSE;
  } else {
    mUiScale          = 16;
    mIconPaddingSize  = 8;
    mIconSpaceSize    = 144;
  }
}

//
// Text rendering
//
STATIC
NDK_UI_IMAGE *
LoadFontImage (
  IN INTN   Cols,
  IN INTN   Rows
  )
{
  NDK_UI_IMAGE                    *NewImage;
  NDK_UI_IMAGE                    *NewFontImage;
  UINT8                           *EmbeddedFontData;
  INTN                            ImageWidth;
  INTN                            ImageHeight;
  INTN                            X;
  INTN                            Y;
  INTN                            Ypos;
  INTN                            J;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL   *PixelPtr;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL   FirstPixel;

  NewImage = NULL;

  if (FileExist (UI_IMAGE_FONT)) {
    NewImage = DecodePNGFile (UI_IMAGE_FONT);
  } else {
    EmbeddedFontData = AllocateCopyPool ((UINTN) emb_font_data_size, (VOID *) &emb_font_data);
    if (EmbeddedFontData == NULL) {
      DEBUG ((DEBUG_ERROR, "Failed to allocate EmbeddedFontData\n"));
      return NULL;
    }

    NewImage = DecodePNG ((VOID *) EmbeddedFontData, (UINT32) emb_font_data_size);
  }

  ImageWidth    = NewImage->Width;
  ImageHeight   = NewImage->Height;
  PixelPtr      = NewImage->Bitmap;
  NewFontImage  = CreateImage ((UINT16) (ImageWidth * Rows), (UINT16) (ImageHeight / Rows), TRUE); // need to be Alpha

  if (NewFontImage == NULL) {
    if (NewImage != NULL) {
      FreeImage (NewImage);
    }
    return NULL;
  }

  mFontWidth  = ImageWidth / Cols;
  mFontHeight = ImageHeight / Rows;
  mTextHeight = mFontHeight + 1;
  FirstPixel  = *PixelPtr;
  for (Y = 0; Y < Rows; ++Y) {
    for (J = 0; J < mFontHeight; J++) {
      Ypos = ((J * Rows) + Y) * ImageWidth;
      for (X = 0; X < ImageWidth; ++X) {
        if ((PixelPtr->Blue == FirstPixel.Blue)
            && (PixelPtr->Green == FirstPixel.Green)
            && (PixelPtr->Red == FirstPixel.Red)) {
          PixelPtr->Reserved = 0;
        } else if (mDarkMode) {
          *PixelPtr = *mFontColorPixel;
        }

        NewFontImage->Bitmap[Ypos + X] = *PixelPtr++;
      }
    }
  }

  FreeImage (NewImage);

  return NewFontImage;
}

STATIC
VOID
PrepareFont (
  VOID
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL   *PixelPtr;
  INTN                            Width;
  INTN                            Height;

  mTextHeight = mFontHeight + 1;

  if (mFontImage != NULL) {
    FreeImage (mFontImage);
    mFontImage = NULL;
  }

  mFontImage = LoadFontImage (16, 16);

  if (mFontImage != NULL) {
    if (!mDarkMode) {
      //invert the font for DarkMode
      PixelPtr = mFontImage->Bitmap;
      for (Height = 0; Height < mFontImage->Height; Height++){
        for (Width = 0; Width < mFontImage->Width; Width++, PixelPtr++){
          PixelPtr->Blue  ^= 0xFF;
          PixelPtr->Green ^= 0xFF;
          PixelPtr->Red   ^= 0xFF;
        }
      }
    }
  } else {
    DEBUG ((DEBUG_INFO, "OCUI: Failed to load font file...\n"));
  }
}

STATIC
BOOLEAN
EmptyPix (
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Ptr,
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL *FirstPixel
  )
{
  //compare with first pixel of the array top-left point [0][0]
   return ((Ptr->Red >= FirstPixel->Red - (FirstPixel->Red >> 2)) && (Ptr->Red <= FirstPixel->Red + (FirstPixel->Red >> 2)) &&
           (Ptr->Green >= FirstPixel->Green - (FirstPixel->Green >> 2)) && (Ptr->Green <= FirstPixel->Green + (FirstPixel->Green >> 2)) &&
           (Ptr->Blue >= FirstPixel->Blue - (FirstPixel->Blue >> 2)) && (Ptr->Blue <= FirstPixel->Blue + (FirstPixel->Blue >> 2)) &&
           (Ptr->Reserved == FirstPixel->Reserved));
}

STATIC
INTN
GetEmpty (
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL    *Ptr,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL    *FirstPixel,
  IN INTN                             MaxWidth,
  IN INTN                             Step,
  IN INTN                             Row
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL   *Ptr0;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL   *Ptr1;
  INTN                            Index;
  INTN                            J;
  INTN                            M;


  Ptr1 = (Step > 0) ? Ptr : Ptr - 1;
  M = MaxWidth;
  for (J = 0; J < mFontHeight; ++J) {
    Ptr0 = Ptr1 + J * Row;
    for (Index = 0; Index < MaxWidth; ++Index) {
      if (!EmptyPix (Ptr0, FirstPixel)) {
        break;
      }
      Ptr0 += Step;
    }
    M = (Index > M) ? M : Index;
  }

  return M;
}

STATIC
INTN
RenderText (
  IN     CHAR16         *Text,
  IN OUT NDK_UI_IMAGE   *CompImage,
  IN     INTN           Xpos,
  IN     INTN           Ypos,
  IN     INTN           Cursor
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL   *BufferPtr;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL   *FontPixelData;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL   *FirstPixelBuf;
  INTN                            BufferLineWidth;
  INTN                            BufferLineOffset;
  INTN                            FontLineOffset;
  INTN                            TextLength;
  INTN                            Index;
  UINT16                          C;
  UINT16                          C0;
  UINT16                          C1;
  UINTN                           Shift;
  UINTN                           LeftSpace;
  UINTN                           RightSpace;
  INTN                            RealWidth;
  INTN                            ScaledWidth;

  ScaledWidth = (INTN) CHAR_WIDTH;
  Shift       = 0;
  RealWidth   = 0;

  TextLength = StrLen (Text);
  if (mFontImage == NULL) {
    PrepareFont();
  }

  BufferPtr         = CompImage->Bitmap;
  BufferLineOffset  = CompImage->Width;
  BufferLineWidth   = BufferLineOffset - Xpos;
  BufferPtr        += Xpos + Ypos * BufferLineOffset;
  FirstPixelBuf     = BufferPtr;
  FontPixelData     = mFontImage->Bitmap;
  FontLineOffset    = mFontImage->Width;

  if (ScaledWidth < mFontWidth) {
    Shift = (mFontWidth - ScaledWidth) >> 1;
  }
  C0 = 0;
  RealWidth = ScaledWidth;
  for (Index = 0; Index < TextLength; ++Index) {
    C   = Text[Index];
    C1  = (((C >= 0xC0) ? (C - (0xC0 - 0xC0)) : C) & 0xff);
    C   = C1;

    if (mProportional) {
      if (C0 <= 0x20) {
        LeftSpace = 2;
      } else {
        LeftSpace = GetEmpty (BufferPtr, FirstPixelBuf, ScaledWidth, -1, BufferLineOffset);
      }

      if (C <= 0x20) {
        RightSpace  = 1;
        RealWidth   = (ScaledWidth >> 1) + 1;
      } else {
        RightSpace = GetEmpty (FontPixelData + C * mFontWidth, FontPixelData, mFontWidth, 1, FontLineOffset);
        if (RightSpace >= ScaledWidth + Shift) {
          RightSpace = 0;
        }
        RealWidth = mFontWidth - RightSpace;
      }
    } else {
      LeftSpace = 2;
      RightSpace = Shift;
    }

    C0 = C;

    if ((UINTN) BufferPtr + RealWidth * 4 > (UINTN) FirstPixelBuf + BufferLineWidth * 4) {
      break;
    }

    RawCompose (BufferPtr - LeftSpace + 2, FontPixelData + C * mFontWidth + RightSpace,
                RealWidth,
                mFontHeight,
                BufferLineOffset,
                FontLineOffset
                );

    if (Index == Cursor) {
      C = 0x5F;

      RawCompose (BufferPtr - LeftSpace + 2, FontPixelData + C * mFontWidth + RightSpace,
                  RealWidth, mFontHeight,
                  BufferLineOffset, FontLineOffset
                  );
    }
    BufferPtr += RealWidth - LeftSpace + 2;
  }

  return ((INTN) BufferPtr - (INTN) FirstPixelBuf) / sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL);
}

STATIC
NDK_UI_IMAGE *
CreateTextImage (
  IN CHAR16   *String
  )
{
  NDK_UI_IMAGE    *Image;
  NDK_UI_IMAGE    *TmpImage;
  NDK_UI_IMAGE    *ScaledTextImage;
  INTN            Width;
  INTN            TextWidth;

  TextWidth = 0;

  if (String == NULL) {
    return NULL;
  }

  Width = ((StrLen (String) + 1) * (INTN) CHAR_WIDTH);
  Image = CreateFilledImage (Width, mTextHeight, TRUE, &mTransparentPixel);
  if (Image != NULL) {
    TextWidth = RenderText (String, Image, 0, 0, 0xFFFF);
  }

  TmpImage = CreateImage ((UINT16) TextWidth, (UINT16) mFontHeight, TRUE);

  RawCopy (TmpImage->Bitmap,
           Image->Bitmap,
           TmpImage->Width,
           TmpImage->Height,
           TmpImage->Width,
           Image->Width
           );

  FreeImage (Image);

  ScaledTextImage = CopyScaledImage (TmpImage, mTextScale);

  if (ScaledTextImage == NULL) {
    DEBUG ((DEBUG_INFO, "OCUI: Failed to scale image!\n"));
    FreeImage (TmpImage);
  }

  return ScaledTextImage;
}

STATIC
VOID
PrintTextGraphicXY (
  IN CHAR16   *String,
  IN INTN     Xpos,
  IN INTN     Ypos
  )
{
  NDK_UI_IMAGE    *TextImage;
  //NDK_UI_IMAGE    *NewImage;
  BOOLEAN         PointerInArea;

  //NewImage = NULL;

  TextImage = CreateTextImage (String);
  if (TextImage == NULL) {
    return;
  }

  if (Xpos < 0) {
    Xpos = (mScreenWidth - TextImage->Width) / 2;
  }

  if ((Xpos + TextImage->Width + 8) > mScreenWidth) {
    Xpos = mScreenWidth - (TextImage->Width + 8);
  }

  if ((Ypos + TextImage->Height + 5) > mScreenHeight) {
    Ypos = mScreenHeight - (TextImage->Height + 5);
  }

  PointerInArea = IsMouseInPlace (Xpos - (POINTER_WIDTH * 2),
                                  (Ypos - POINTER_HEIGHT) > 0 ? Ypos - POINTER_HEIGHT : 0,
                                  TextImage->Width + (POINTER_WIDTH * 2),
                                  TextImage->Height + POINTER_HEIGHT
                                  );

  if (PointerInArea) {
    HidePointer ();
  }

  BltImageAlpha (TextImage, Xpos, Ypos, &mTransparentPixel, 16);

  if (PointerInArea) {
    DrawPointer ();
  }
}

//
//     Text rendering end
//

STATIC
VOID
PrintLabel (
  IN OC_BOOT_ENTRY    **Entries,
  IN UINTN            *VisibleList,
  IN UINTN            VisibleIndex,
  IN INTN             Xpos,
  IN INTN             Ypos
  )
{
  NDK_UI_IMAGE    *TextImage;
  NDK_UI_IMAGE    *NewImage;
  NDK_UI_IMAGE    *LabelImage;
  UINTN           Index;
  UINTN           Needle;
  CHAR16          *String;
  UINTN           Length;
  //INTN            Rows;
  INTN            IconsPerRow;
  INTN            NewXpos;
  INTN            NewYpos;

  Length        = (144 / (INTN) CHAR_WIDTH) - 2;
  //Rows          = mMenuImage->Height / mIconSpaceSize;
  IconsPerRow   = mMenuImage->Width / mIconSpaceSize;
  NewXpos       = Xpos;
  NewYpos       = Ypos;

  for (Index = 0; Index < VisibleIndex; ++Index) {
    if (StrLen (Entries[VisibleList[Index]]->Name) > Length) {
      String = AllocateZeroPool ((Length + 1) * sizeof (CHAR16));
      StrnCpyS (String, Length + 1, Entries[VisibleList[Index]]->Name, Length);

      for (Needle = Length; Needle > 0; --Needle) {
        if (String[Needle] == 0x20) {
          StrnCpyS (String, Needle + 1, Entries[VisibleList[Index]]->Name, Needle);
          break;
        }
      }

      TextImage = CreateTextImage (String);

      FreePool (String);
    } else {
      TextImage = CreateTextImage (Entries[VisibleList[Index]]->Name);
    }

    if (TextImage == NULL) {
      return;
    }

    LabelImage = CopyScaledImage (mLabelImage, (mIconSpaceSize << 4) / mLabelImage->Width);

    NewImage = CreateImage (LabelImage->Width, LabelImage->Height, FALSE);

    if (Index == (UINTN)IconsPerRow) {
      NewXpos = Xpos;
      NewYpos = Ypos + mIconSpaceSize + (32 * mUiScale >> 4) + ICON_ROW_SPACE_OFFSET;
    }

    TakeImage (NewImage, NewXpos, NewYpos + mIconSpaceSize + 10, LabelImage->Width, LabelImage->Height);

    RawCompose (NewImage->Bitmap,
                LabelImage->Bitmap,
                NewImage->Width,
                NewImage->Height,
                NewImage->Width,
                LabelImage->Width
                );

    FreeImage (LabelImage);

    ComposeImage (NewImage, TextImage, (NewImage->Width - TextImage->Width) >> 1, (NewImage->Height - TextImage->Height) >> 1);

    DrawImageArea (NewImage, 0, 0, 0, 0, NewXpos, NewYpos + mIconSpaceSize + 10);

    FreeImage (TextImage);
    FreeImage (NewImage);

    NewXpos = NewXpos + mIconSpaceSize;
  }
}

STATIC
VOID
PrintDateTime (
  IN BOOLEAN    ShowAll
  )
{
  EFI_STATUS    Status;
  EFI_TIME      DateTime;
  CHAR16        DateStr[12];
  CHAR16        TimeStr[12];
  UINTN         Hour;
  CHAR16        *Str;
  INTN          Width;

  Str   = NULL;
  Hour  = 0;

  if (ShowAll) {
    Status = gRT->GetTime (&DateTime, NULL);
    if (EFI_ERROR (Status)) {
      ZeroMem (&DateTime, sizeof (DateTime));
    }

    Hour  = (UINTN) DateTime.Hour;
    Str   = Hour >= 12 ? L"PM" : L"AM";
    if (Hour > 12) {
      Hour = Hour - 12;
    }

    UnicodeSPrint (DateStr, sizeof (DateStr), L" %02u/%02u/%04u", DateTime.Month, DateTime.Day, DateTime.Year);
    UnicodeSPrint (TimeStr, sizeof (TimeStr), L" %02u:%02u:%02u%s", Hour, DateTime.Minute, DateTime.Second, Str);

    Width = (((StrLen (DateStr) + 1) * (INTN) CHAR_WIDTH) * mTextScale) >> 4;

    ClearScreenArea (&mTransparentPixel, mScreenWidth - Width, 0, Width, ((mTextHeight * mTextScale) >> 4) * 2 + 5);

    PrintTextGraphicXY (DateStr, mScreenWidth, 5);
    PrintTextGraphicXY (TimeStr, mScreenWidth, ((mTextHeight * mTextScale) >> 4) + 5);
  } else {
    ClearScreenArea (&mTransparentPixel, mScreenWidth - (mScreenWidth / 5), 0, mScreenWidth / 5, ((mTextHeight * mTextScale) >> 4) * 2 + 5);
  }
}

STATIC
VOID
PrintOcVersion (
  IN CONST CHAR8    *String,
  IN BOOLEAN        ShowAll
  )
{
  CHAR16  *NewString;

  if (String == NULL) {
    return;
  }

  NewString = AsciiStrCopyToUnicode (String, 0);
  if (NewString != NULL) {
    if (ShowAll) {
      PrintTextGraphicXY (NewString, mScreenWidth, mScreenHeight);
    } else {
      ClearScreenArea (&mTransparentPixel,
                         mScreenWidth - ((StrLen(NewString) * mFontWidth) * 2),
                         mScreenHeight - mFontHeight * 2,
                         (StrLen(NewString) * mFontWidth) * 2,
                         mFontHeight * 2
                         );
    }

    FreePool (NewString);
  }
}

STATIC
BOOLEAN
PrintTimeOutMessage (
  IN UINTN    Timeout
  )
{
  CHAR16    String[52];

  if (Timeout > 0 && !mPointerIsActive) {
    UnicodeSPrint (String, sizeof (String), L"%s %02u %s.", L"The default boot selection will start in", Timeout, L"seconds");
    PrintTextGraphicXY (String, -1, (mScreenHeight / 4) * 3);
  } else {
    ClearScreenArea (&mTransparentPixel, 0, ((mScreenHeight / 4) * 3) - 4, mScreenWidth, mFontHeight * 2);
    Timeout = 0;
  }

  return !(Timeout > 0);
}

STATIC
VOID
PrintTextDescription (
  IN UINTN            MaxStrWidth,
  IN UINTN            Selected,
  IN OC_BOOT_ENTRY    *Entry
  )
{
  NDK_UI_IMAGE    *TextImage;
  NDK_UI_IMAGE    *NewImage;
  CHAR16          Code[3];
  CHAR16          *String;

  if (mPrintLabel || MaxStrWidth == 0) {
    return;
  }

  Code[0] = 0x20;
  Code[1] = OC_INPUT_STR[Selected];
  Code[2] = '\0';

  String = AllocateZeroPool ((MaxStrWidth + 1) * sizeof (CHAR16));
  if (String == NULL) {
    return;
  }

  UnicodeSPrint (String, sizeof (String), L" %s%s%s%s%s ",
                 Code,
                 (mAllowSetDefault && mDefaultEntry == Selected) ? L".*" : L". ",
                 Entry->Name,
                 Entry->IsExternal ? L" (ext)" : L"",
                 Entry->IsFolder ? L" (dmg)" : L""
                 );

  TextImage = CreateTextImage (String);
  if (TextImage == NULL) {
    FreePool (String);
    return;
  }

  NewImage = CreateFilledImage (mScreenWidth, TextImage->Height, TRUE, &mTransparentPixel);
  if (NewImage == NULL) {
    FreeImage (TextImage);
    FreePool (String);
    return;
  }

  ComposeImage (NewImage, TextImage, (NewImage->Width - TextImage->Width) / 2, 0);

  FreeImage (TextImage);

  BltImageAlpha (NewImage,
                 (mScreenWidth - NewImage->Width) / 2,
                 (mScreenHeight / 2) + mIconSpaceSize,
                 &mTransparentPixel,
                 16
                 );

  FreePool (String);
}

STATIC
VOID
CreateToolBar (
  IN BOOLEAN    Initialize
  )
{
  NDK_UI_IMAGE    *LabelImage;
  NDK_UI_IMAGE    *Icon;
  INTN            IconScale;
  INTN            Offset;

  IconScale = 16;
  if (mScreenHeight < 2160) {
    IconScale = 8;
  } else if (mScreenHeight <= 800) {
    IconScale = 4;
  }

  if (Initialize) {
    if (mIconReset.Image == NULL && FileExist (UI_ICON_RESET)) {
      Icon = DecodePNGFile (UI_ICON_RESET);
      mIconReset.Image = CopyScaledImage (Icon,IconScale);
      FreeImage (Icon);
    } else {
      mIconReset.Image = CreateFilledImage (80, 80, TRUE, &mBluePixel);
    }

    if (mIconReset.Selector == NULL && FileExist (UI_IMAGE_SELECTOR_FUNC)) {
      Icon = DecodePNGFile (UI_IMAGE_SELECTOR_FUNC);
      mIconReset.Selector = CopyScaledImage (Icon, IconScale);
      FreeImage (Icon);
    } else {
      //mIconReset.Selector = mIconReset.Image;
      mIconReset.Selector = AllocateCopyPool (sizeof (NDK_UI_IMAGE), mIconReset.Image);
      //mIconReset.Selector = CreateFilledImage (80, 80, TRUE, &mBluePixel);
    }

    if (mIconShutdown.Image == NULL && FileExist (UI_ICON_SHUTDOWN)) {
      Icon = DecodePNGFile (UI_ICON_SHUTDOWN);
      mIconShutdown.Image = CopyScaledImage (Icon, IconScale);
      FreeImage (Icon);
    } else {
      //mIconShutdown.Image = mIconReset.Image;
      mIconShutdown.Image = AllocateCopyPool (sizeof (NDK_UI_IMAGE), mIconReset.Image);
      //mIconShutdown.Image = CreateFilledImage (80, 80, TRUE, &mBluePixel);
    }

    mIconReset.Xpos       = mScreenWidth / 2 - (mIconReset.Image->Width + (mIconReset.Image->Width >> 2));
    mIconReset.Ypos       = mScreenHeight - (mIconReset.Image->Width * 2);
    mIconReset.Action     = SystemReset;

    mIconShutdown.Xpos    = mScreenWidth / 2 + (mIconShutdown.Image->Width >> 2);
    mIconShutdown.Ypos    = mIconReset.Ypos;
    mIconShutdown.Action  = SystemReset;
  }

  BltMenuImage (mIconReset.Image, mIconReset.Xpos, mIconReset.Ypos);
  BltMenuImage (mIconShutdown.Image, mIconShutdown.Xpos, mIconShutdown.Ypos);

  LabelImage  = CreateTextImage (UI_MENU_SYSTEM_RESET);
  Offset      = (mIconReset.Image->Width - LabelImage->Width) >> 1;
  BltImageAlpha (LabelImage,
                 mIconReset.Xpos + ABS (Offset),
                 mIconReset.Ypos + mIconReset.Image->Height - IconScale,
                 &mTransparentPixel,
                 16
                 );

  LabelImage  = CreateTextImage (UI_MENU_SYSTEM_SHUTDOWN);
  Offset      = (mIconShutdown.Image->Width - LabelImage->Width) >> 1;
  BltImageAlpha (LabelImage,
                 mIconShutdown.Xpos + ABS (Offset),
                 mIconShutdown.Ypos + mIconShutdown.Image->Height - IconScale,
                 &mTransparentPixel,
                 16
                 );
}

STATIC
VOID
FreeToolBar (
  VOID
  )
{
  FreeImage (mIconReset.Image);
  mIconReset.Image          = NULL;
  mIconReset.IsSelected     = FALSE;
  FreeImage (mIconReset.Selector);
  mIconReset.Selector       = NULL;
  mIconReset.Action         = NULL;
  FreeImage (mIconShutdown.Image);
  mIconShutdown.IsSelected  = FALSE;
  mIconShutdown.Image       = NULL;
  mIconShutdown.Action      = NULL;
}

STATIC
VOID
SelectResetFunc (
  VOID
  )
{
  NDK_UI_IMAGE    *NewImage;

  HidePointer ();

  CreateToolBar (FALSE);

  if (!mIconReset.IsSelected && !mIconShutdown.IsSelected) {
    DrawPointer ();
    return;
  }

  NewImage = CreateImage (mIconReset.Image->Width, mIconReset.Image->Height, FALSE);

  if (mIconReset.IsSelected) {
    TakeImage (NewImage, mIconReset.Xpos, mIconReset.Ypos, NewImage->Width, NewImage->Height);
  } else if (mIconShutdown.IsSelected) {
    TakeImage (NewImage, mIconShutdown.Xpos, mIconShutdown.Ypos, NewImage->Width, NewImage->Height);
  }

  RawCompose (NewImage->Bitmap,
              mIconReset.Selector->Bitmap,
              NewImage->Width,
              NewImage->Height,
              NewImage->Width,
              mIconReset.Selector->Width
              );

  if (mIconReset.IsSelected) {
    BltImage (NewImage, mIconReset.Xpos, mIconReset.Ypos);
  } else if (mIconShutdown.IsSelected) {
    BltImage (NewImage, mIconShutdown.Xpos, mIconShutdown.Ypos);
  }

  FreeImage (NewImage);

  DrawPointer ();
}

STATIC
INTN
CheckIconClick (
  VOID
  )
{
  INTN       IconsPerRow;
  //INTN       Rows;
  INTN       Xpos;
  INTN       Ypos;
  INTN       NewXpos;
  INTN       NewYpos;
  INTN       Result;
  UINTN      Index;
  AREA_RECT  Place;

  Result = -1;

  if (mMenuImage == NULL) {
    return Result;
  }

  Place.Width   = mIconSpaceSize;
  Place.Height  = mIconSpaceSize;

  //Rows          = mMenuImage->Height / mIconSpaceSize;
  IconsPerRow   = mMenuImage->Width / mIconSpaceSize;
  Xpos          = (mScreenWidth - mMenuImage->Width) / 2;
  Ypos          = (mScreenHeight / 2) - mIconSpaceSize;
  NewXpos       = Xpos;
  NewYpos       = Ypos;

  mPointer.IsClickable = FALSE;

  for (Index = 0; Index < (UINTN) mMenuIconsCount; ++Index) {
    Place.Xpos = NewXpos;
    Place.Ypos = NewYpos;

    if (MouseInRect (&Place)) {
      mPointer.IsClickable = TRUE;
      Result = (INTN) Index;
      break;
    }

    NewXpos = NewXpos + mIconSpaceSize;

    if (Index == (UINTN) (IconsPerRow - 1)) {
      NewXpos = Xpos;
      NewYpos = Ypos + mIconSpaceSize + (32 * mUiScale >> 4) + ICON_ROW_SPACE_OFFSET;
    }
  }

  Place.Width   = mIconReset.Image->Width;
  Place.Height  = mIconReset.Image->Width;
  Place.Xpos    = mIconReset.Xpos;
  Place.Ypos    = mIconReset.Ypos;

  if (MouseInRect (&Place)) {
    mPointer.IsClickable = TRUE;

    if (!mIconReset.IsSelected) {
      mIconReset.IsSelected     = TRUE;
      mIconShutdown.IsSelected  = FALSE;

      SelectResetFunc ();
    }

    Result = UI_INPUT_SYSTEM_RESET;
  }

  Place.Xpos = mIconShutdown.Xpos;
  Place.Ypos = mIconShutdown.Ypos;

  if (MouseInRect (&Place)) {
    mPointer.IsClickable = TRUE;
    if (!mIconShutdown.IsSelected) {
      mIconShutdown.IsSelected  = TRUE;
      mIconReset.IsSelected     = FALSE;

      SelectResetFunc ();
    }

    Result = UI_INPUT_SYSTEM_SHUTDOWN;
  }

  if (!mPointer.IsClickable && (mIconShutdown.IsSelected || mIconReset.IsSelected)) {
    mIconShutdown.IsSelected  = FALSE;
    mIconReset.IsSelected     = FALSE;

    SelectResetFunc ();
  }

  return Result;
}

STATIC
VOID
KillMouse (
  VOID
  )
{
  if (mPointer.SimplePointerProtocol == NULL) {
    return;
  }

  FreeImage (mPointer.NewImage);
  FreeImage (mPointer.OldImage);

  if (mPointer.Pointer != NULL) {
    FreeImage (mPointer.Pointer);
  }

  if (mPointer.PointerAlt != NULL) {
    FreeImage (mPointer.PointerAlt);
  }

  mPointer.IsClickable            = FALSE;
  mPointer.MouseEvent             = NoEvents;
  mPointer.NewImage               = NULL;
  mPointer.OldImage               = NULL;
  mPointer.Pointer                = NULL;
  mPointer.PointerAlt             = NULL;
  mPointer.SimplePointerProtocol  = NULL;

  mPointerIsActive = FALSE;
}

/* Mouse Functions End*/

STATIC
INTN
OcWaitForKeyIndex (
  IN OUT OC_PICKER_CONTEXT                  *Context,
  IN     APPLE_KEY_MAP_AGGREGATOR_PROTOCOL  *KeyMap,
  IN     UINTN                              Timeout,
  IN     BOOLEAN                            PollHotkeys,
     OUT BOOLEAN                            *SetDefault  OPTIONAL
  )
{
  UINT64    CurrTime;
  UINT64    EndTime;
  INTN      KeyClick;
  INTN      CycleCount;
  INTN      ResultingKey;

  CycleCount = 0;

  CurrTime  = GetTimeInNanoSecond (GetPerformanceCounter ());
  EndTime   = CurrTime + Timeout * 1000000ULL;

  if (SetDefault != NULL) {
    *SetDefault = FALSE;
  }

  while (Timeout == 0 || CurrTime == 0 || CurrTime < EndTime) {
    if (mPointer.SimplePointerProtocol != NULL) {
      PointerUpdate();
      switch (mPointer.MouseEvent) {
        case DoubleClick:
        case LeftClick:
          mPointer.MouseEvent = NoEvents;
          KeyClick = CheckIconClick ();
          if (KeyClick == UI_INPUT_SYSTEM_RESET) {
            mIconReset.Action (EfiResetCold);
          }
          if (KeyClick == UI_INPUT_SYSTEM_SHUTDOWN) {
            mIconShutdown.Action (EfiResetShutdown);
          }
          if (KeyClick >= 0) {
            return KeyClick;
          }
          break;
        case RightClick:
          mPointer.MouseEvent = NoEvents;
          return OC_INPUT_MORE;
        case MouseMove:
          CycleCount++;
          mPointer.MouseEvent = NoEvents;
          if (CycleCount == 10) {
            KeyClick = CheckIconClick ();
            if (KeyClick == UI_INPUT_SYSTEM_RESET || KeyClick == UI_INPUT_SYSTEM_SHUTDOWN) {
              mCurrentSelection = UI_INPUT_SYSTEM_RESET;
              return OC_INPUT_TAB;
            } else if (KeyClick >= 0 && KeyClick != mCurrentSelection) {
              mCurrentSelection = KeyClick;
              return OC_INPUT_POINTER;
            }
            CycleCount = 0;
          }
          break;
        default:
          break;
      }

      mPointer.MouseEvent = NoEvents;
    }

    CurrTime      = GetTimeInNanoSecond (GetPerformanceCounter ());
    ResultingKey  = OcGetAppleKeyIndex (Context, KeyMap, SetDefault);

    //
    // Requested for another iteration, handled Apple hotkey.
    //
    if (ResultingKey == OC_INPUT_INTERNAL) {
      continue;
    }

    //
    // Abort the timeout when unrecognised keys are pressed.
    //
    if (Timeout != 0 && ResultingKey == OC_INPUT_INVALID) {
      return OC_INPUT_INVALID;
    }

    //
    // Found key, return it.
    //
    if (ResultingKey != OC_INPUT_INVALID && ResultingKey != OC_INPUT_TIMEOUT) {
      return ResultingKey;
    }

    MicroSecondDelay (10);
  }

  return OC_INPUT_TIMEOUT;
}

STATIC
VOID
RestoreConsoleMode (
  IN OC_PICKER_CONTEXT    *Context
  )
{
  FreeImage (mBackgroundImage);
  FreeImage (mFontImage);
  FreeImage (mLabelImage);
  FreeImage (mMenuImage);
  FreeImage (mSelectionImage);

  mFontImage      = NULL;
  mLabelImage     = NULL;
  mMenuImage      = NULL;
  mSelectionImage = NULL;

  FreeToolBar ();

  ClearScreenArea (&mBlackPixel, 0, 0, mScreenWidth, mScreenHeight);

  mUiScale    = 0;
  mTextScale  = 0;

  if (Context->ConsoleAttributes != 0) {
    gST->ConOut->SetAttribute (gST->ConOut, Context->ConsoleAttributes & 0x7FU);
  }
  gST->ConOut->SetCursorPosition (gST->ConOut, 0, 0);

  KillMouse ();
}

STATIC
VOID
CheckTabSelect (
  IN  INTN    *KeyIndex
  )
{
  if (KeyIndex != NULL) {
    if (*KeyIndex == OC_INPUT_BOTTOM) {
      if (mIconReset.IsSelected || (!mIconReset.IsSelected && !mIconShutdown.IsSelected)) {
        mIconReset.IsSelected     = !mIconReset.IsSelected;
        mIconShutdown.IsSelected  = !mIconReset.IsSelected;
        *KeyIndex = OC_INPUT_TAB;
      } else {
        mIconReset.IsSelected = mIconShutdown.IsSelected = FALSE;
        *KeyIndex = OC_INPUT_MENU;
      }
      SelectResetFunc ();
    } else if (*KeyIndex == OC_INPUT_TOP) {
      if (mIconShutdown.IsSelected || (!mIconReset.IsSelected && !mIconShutdown.IsSelected)) {
        mIconShutdown.IsSelected  = !mIconShutdown.IsSelected;
        mIconReset.IsSelected     = !mIconShutdown.IsSelected;
        *KeyIndex = OC_INPUT_TAB;
      } else {
        mIconShutdown.IsSelected = mIconReset.IsSelected = FALSE;
        *KeyIndex = OC_INPUT_MENU;
      }
      SelectResetFunc ();
    }
  }
}

STATIC
EFI_STATUS
UiMenuMain (
  IN OC_BOOT_CONTEXT    *BootContext,
  IN OC_BOOT_ENTRY      **BootEntries,
  OUT OC_BOOT_ENTRY     **ChosenBootEntry
  )
{
  EFI_STATUS                          Status;
  UINTN                               Index;
  //UINTN                               CustomEntryIndex;
  INTN                                KeyIndex;
  UINT32                              TimeOutSeconds;
  UINTN                               Count;
  UINTN                               *VisibleList;
  UINTN                               VisibleIndex;
  BOOLEAN                             ShowAll;
  UINTN                               Selected;
  UINTN                               MaxStrWidth;
  UINTN                               StrWidth;
  APPLE_KEY_MAP_AGGREGATOR_PROTOCOL   *KeyMap;
  BOOLEAN                             SetDefault;
  BOOLEAN                             TimeoutExpired;
  BOOLEAN                             PlayedOnce;
  BOOLEAN                             PlayChosen;
  UINTN                               DefaultEntry;
  OC_PICKER_CONTEXT                   *PickerContext;

  PickerContext     = BootContext->PickerContext;
  DefaultEntry      = BootContext->DefaultEntry->EntryIndex - 1;
  Count             = BootContext->BootEntryCount;

  mHideAuxiliary    = PickerContext->HideAuxiliary;

  Selected          = 0;
  VisibleIndex      = 0;
  MaxStrWidth       = 0;
  TimeoutExpired    = FALSE;
  ShowAll           = !mHideAuxiliary;
  TimeOutSeconds    = PickerContext->TimeoutSeconds;
  mAllowSetDefault  = PickerContext->AllowSetDefault;
  //mStorage           = PickerContext->CustomEntryPickerContext;
  mDefaultEntry     = DefaultEntry;
  //CustomEntryIndex  = 0;
  PlayedOnce        = FALSE;
  PlayChosen        = FALSE;

  //if (mStorage->FileSystem != NULL) {
  //  DEBUG ((DEBUG_INFO, "OCUI: FileSystem Found!\n"));
  //}

  KeyMap = OcAppleKeyMapInstallProtocols (FALSE);
  if (KeyMap == NULL) {
    DEBUG ((DEBUG_ERROR, "OCUI: Missing AppleKeyMapAggregator\n"));
    return EFI_UNSUPPORTED;
  }

  for (Index = 0; Index < MIN (Count, OC_INPUT_MAX); ++Index) {
    StrWidth    = StrLen (BootEntries[Index]->Name) + ((BootEntries[Index]->IsFolder || BootEntries[Index]->IsExternal) ? 11 : 5);
    MaxStrWidth = MaxStrWidth > StrWidth ? MaxStrWidth : StrWidth;
    //if (BootEntries[Index]->Type == OC_BOOT_EXTERNAL_OS || BootEntries[Index]->Type == OC_BOOT_EXTERNAL_TOOL) {
    //  BootEntries[Index]->IsAuxiliary = PickerContext->CustomEntries[CustomEntryIndex].Auxiliary;
    //  ++CustomEntryIndex;
    //}
  }

  VisibleList = AllocatePool (Count * sizeof (UINTN));
  if (VisibleList == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  InitScreen ();
  ClearScreen (&mTransparentPixel);
  PrepareFont ();
  CreateToolBar (TRUE);

  while (TRUE) {
    FreeImage (mMenuImage);

    mMenuImage = NULL;

    for (Index = 0, VisibleIndex = 0; Index < MIN (Count, OC_INPUT_MAX); ++Index) {
      if ((BootEntries[Index]->Type == OC_BOOT_APPLE_RECOVERY && !ShowAll)
          || (BootEntries[Index]->Type == OC_BOOT_APPLE_TIME_MACHINE && !ShowAll)
          || (BootEntries[Index]->DevicePath == NULL && !ShowAll)
          //|| (BootEntries[Index]->IsAuxiliary && !ShowAll)
          )
      {
        DefaultEntry = DefaultEntry == Index ? DefaultEntry + 1 : DefaultEntry;
        continue;
      }

      if (DefaultEntry == Index) {
        Selected = VisibleIndex;
      }

      VisibleList[VisibleIndex] = Index;

      CreateIcon (BootEntries[Index]->Name,
                  BootEntries[Index]->Type,
                  VisibleIndex,
                  BootEntries[Index]->IsExternal,
                  BootEntries[Index]->IsFolder
                  );

      ++VisibleIndex;
    }

    ClearScreenArea (&mTransparentPixel, 0, (mScreenHeight / 2) - (mIconSpaceSize + 20), mScreenWidth, mIconSpaceSize * 3);

    BltMenuImage (mMenuImage, (mScreenWidth - mMenuImage->Width) / 2, (mScreenHeight / 2) - mIconSpaceSize);

    if (mPrintLabel) {
      PrintLabel (BootEntries, VisibleList, VisibleIndex, (mScreenWidth - mMenuImage->Width) / 2, (mScreenHeight / 2) - mIconSpaceSize);
    }

    PrintTextDescription (MaxStrWidth,
                          Selected,
                          BootEntries[DefaultEntry]
                          );

    SwitchIconSelection (VisibleIndex, Selected, TRUE, FALSE);

    mCurrentSelection = Selected;
    mMenuIconsCount   = VisibleIndex;

    PrintOcVersion (PickerContext->TitleSuffix, ShowAll);

    PrintDateTime (ShowAll);

    if (!TimeoutExpired) {
      TimeoutExpired = PrintTimeOutMessage (TimeOutSeconds);
      TimeOutSeconds = TimeoutExpired ? 10000 : TimeOutSeconds;
    }

    if ((PickerContext->PickerAttributes & OC_ATTR_USE_POINTER_CONTROL) != 0) {
      InitMouse ();
    }

    if (ShowAll && PlayedOnce) {
      OcPlayAudioFile (PickerContext, OcVoiceOverAudioFileShowAuxiliary, FALSE);
    }

    if (!PlayedOnce && PickerContext->PickerAudioAssist) {
      OcPlayAudioFile (PickerContext, OcVoiceOverAudioFileChooseOS, FALSE);
      for (Index = 0; Index < VisibleIndex; ++Index) {
        OcPlayAudioEntry (PickerContext, BootEntries[VisibleList[Index]]);
        if (DefaultEntry == VisibleList[Index] && TimeOutSeconds > 0) {
          OcPlayAudioFile (PickerContext, OcVoiceOverAudioFileDefault, FALSE);
        }
      }

      OcPlayAudioBeep (
        PickerContext,
        OC_VOICE_OVER_SIGNALS_NORMAL,
        OC_VOICE_OVER_SIGNAL_NORMAL_MS,
        OC_VOICE_OVER_SILENCE_NORMAL_MS
        );

      PlayedOnce = TRUE;
    }

    while (TRUE) {
      KeyIndex = OcWaitForKeyIndex (PickerContext, KeyMap, 1000, PickerContext->PollAppleHotKeys, &SetDefault);

      CheckTabSelect (&KeyIndex);

      if (PlayChosen && KeyIndex == OC_INPUT_TIMEOUT) {
        OcPlayAudioFile (PickerContext, OcVoiceOverAudioFileSelected, FALSE);
        OcPlayAudioEntry (PickerContext, BootEntries[DefaultEntry]);

        PlayChosen = FALSE;
      }

      --TimeOutSeconds;

      if ((KeyIndex == OC_INPUT_TIMEOUT && TimeOutSeconds == 0) || KeyIndex == OC_INPUT_CONTINUE) {
        if (mIconReset.IsSelected) {
          mIconReset.Action (EfiResetCold);
        } else if (mIconShutdown.IsSelected) {
          mIconShutdown.Action (EfiResetShutdown);
        }

        SwitchIconSelection (VisibleIndex, Selected, TRUE, TRUE);

        *ChosenBootEntry = BootEntries[DefaultEntry];

        SetDefault = BootEntries[DefaultEntry]->DevicePath != NULL
          //&& !BootEntries[DefaultEntry]->IsAuxiliary
          && PickerContext->AllowSetDefault
          && SetDefault;
        if (SetDefault) {
          OcPlayAudioFile (PickerContext, OcVoiceOverAudioFileSelected, FALSE);
          OcPlayAudioFile (PickerContext, OcVoiceOverAudioFileDefault, FALSE);
          OcPlayAudioEntry (PickerContext, BootEntries[DefaultEntry]);

          Status = OcSetDefaultBootEntry (PickerContext, BootEntries[DefaultEntry]);
          DEBUG ((DEBUG_INFO, "OCUI: Setting default - %r\n", Status));
        }

        RestoreConsoleMode (PickerContext);

        FreePool (VisibleList);

        return EFI_SUCCESS;
      } else if (KeyIndex == OC_INPUT_ABORTED) {
        TimeOutSeconds = 0;

        OcPlayAudioFile (PickerContext, OcVoiceOverAudioFileAbortTimeout, FALSE);

        break;
      } else if (KeyIndex == OC_INPUT_FUNCTIONAL(10)) {
        TimeOutSeconds = 0;

        TakeScreenShot (L"ScreenShot");
      } else if (KeyIndex == OC_INPUT_MORE && !mIconReset.IsSelected && !mIconShutdown.IsSelected) {
        HidePointer ();

        TimeOutSeconds  = 0;
        ShowAll         = !ShowAll;
        mHideAuxiliary  = !ShowAll;
        DefaultEntry    = mDefaultEntry;

        break;
      } else if (KeyIndex == OC_INPUT_MENU) {
        HidePointer ();

        SwitchIconSelection (VisibleIndex, Selected, TRUE, FALSE);

        PrintTextDescription (MaxStrWidth,
                              Selected,
                              BootEntries[DefaultEntry]
                              );

        PlayChosen = PickerContext->PickerAudioAssist;

        DrawPointer ();
      } else if (KeyIndex == OC_INPUT_TAB) {
        HidePointer ();

        TimeOutSeconds = 0;

        SwitchIconSelection (VisibleIndex, Selected, FALSE, FALSE);

        if (!mPrintLabel) {
          ClearScreenArea (&mTransparentPixel, 0, (mScreenHeight / 2) + mIconSpaceSize, mScreenWidth, mIconSpaceSize);
        }

        DrawPointer ();
      } else if ((KeyIndex == OC_INPUT_UP && !mIconReset.IsSelected && !mIconShutdown.IsSelected)
                 || (KeyIndex == OC_INPUT_LEFT && !mIconReset.IsSelected && !mIconShutdown.IsSelected)) {
        HidePointer ();

        TimeOutSeconds  = 0;

        SwitchIconSelection (VisibleIndex, Selected, FALSE, FALSE);

        DefaultEntry      = Selected > 0 ? VisibleList[Selected - 1] : VisibleList[VisibleIndex - 1];
        Selected          = Selected > 0 ? Selected - 1 : VisibleIndex - 1;
        mCurrentSelection = Selected;

        SwitchIconSelection (VisibleIndex, Selected, TRUE, FALSE);

        PrintTextDescription (MaxStrWidth,
                              Selected,
                              BootEntries[DefaultEntry]
                              );

        PlayChosen = PickerContext->PickerAudioAssist;

        DrawPointer ();
      } else if ((KeyIndex == OC_INPUT_DOWN && !mIconReset.IsSelected && !mIconShutdown.IsSelected)
                 || (KeyIndex == OC_INPUT_RIGHT && !mIconReset.IsSelected && !mIconShutdown.IsSelected)) {
        HidePointer ();

        TimeOutSeconds  = 0;

        SwitchIconSelection (VisibleIndex, Selected, FALSE, FALSE);

        DefaultEntry      = Selected < (VisibleIndex - 1) ? VisibleList[Selected + 1] : VisibleList[0];
        Selected          = Selected < (VisibleIndex - 1) ? Selected + 1 : 0;
        mCurrentSelection = Selected;

        SwitchIconSelection (VisibleIndex, Selected, TRUE, FALSE);

        PrintTextDescription (MaxStrWidth,
                              Selected,
                              BootEntries[DefaultEntry]
                              );

        PlayChosen = PickerContext->PickerAudioAssist;

        DrawPointer ();
      } else if (KeyIndex == OC_INPUT_POINTER) {
        HidePointer ();

        TimeOutSeconds  = 0;

        SwitchIconSelection (VisibleIndex, Selected, FALSE, FALSE);

        Selected      = mCurrentSelection;
        DefaultEntry  = VisibleList[Selected];

        SwitchIconSelection (VisibleIndex, Selected, TRUE, FALSE);

        PrintTextDescription (MaxStrWidth,
                              Selected,
                              BootEntries[DefaultEntry]
                              );
        PlayChosen = PickerContext->PickerAudioAssist;

        DrawPointer ();
      } else if (KeyIndex != OC_INPUT_INVALID && (UINTN)KeyIndex < VisibleIndex) {
        ASSERT (KeyIndex >= 0);

        SwitchIconSelection (VisibleIndex, Selected, TRUE, TRUE);

        *ChosenBootEntry  = BootEntries[VisibleList[KeyIndex]];

        SetDefault        = BootEntries[VisibleList[KeyIndex]]->DevicePath != NULL
          //&& !BootEntries[VisibleList[KeyIndex]]->IsAuxiliary
          && PickerContext->AllowSetDefault
          && SetDefault;
        if (SetDefault) {
          OcPlayAudioFile (PickerContext, OcVoiceOverAudioFileSelected, FALSE);
          OcPlayAudioFile (PickerContext, OcVoiceOverAudioFileDefault, FALSE);
          OcPlayAudioEntry (PickerContext, BootEntries[VisibleList[KeyIndex]]);

          Status = OcSetDefaultBootEntry (PickerContext, BootEntries[VisibleList[KeyIndex]]);
          DEBUG ((DEBUG_INFO, "OCUI: Setting default - %r\n", Status));
        }

        RestoreConsoleMode (PickerContext);

        FreePool (VisibleList);

        return EFI_SUCCESS;
      } else if (KeyIndex == OC_INPUT_VOICE_OVER) {
        TimeOutSeconds = 0;

        OcToggleVoiceOver (PickerContext, 0);
        break;
      } else if (KeyIndex != OC_INPUT_TIMEOUT) {
        TimeOutSeconds = 0;
      }

      if (!TimeoutExpired) {
        PrintDateTime (ShowAll);

        TimeoutExpired = PrintTimeOutMessage (TimeOutSeconds);
        TimeOutSeconds = TimeoutExpired ? 10000 : TimeOutSeconds;
      } else {
        PrintDateTime (ShowAll);
      }
    }
  }

  FreePool (VisibleList);

  ASSERT (FALSE);
}

STATIC
EFI_STATUS
EFIAPI
OcShowMenuByOc (
  IN  OC_BOOT_CONTEXT    *BootContext,
  IN  OC_BOOT_ENTRY      **BootEntries,
  OUT OC_BOOT_ENTRY      **ChosenBootEntry
  )
{
  EFI_STATUS    Status;

  *ChosenBootEntry = NULL;

  //
  // Extension for OpenCore builtin renderer to mark that we control text output here.
  //
  gST->ConOut->TestString (gST->ConOut, OC_CONSOLE_MARK_CONTROLLED);

  Status = UiMenuMain (
    BootContext,
    BootEntries,
    ChosenBootEntry
    );

  //
  // Extension for OpenCore builtin renderer to mark that we no longer control text output here.
  //
  gST->ConOut->TestString (gST->ConOut, OC_CONSOLE_MARK_UNCONTROLLED);

  BootContext->PickerContext->HideAuxiliary = mHideAuxiliary;

  //return EFI_SUCCESS;
  return Status;
}

STATIC
EFI_STATUS
EFIAPI
GuiOcInterfacePopulate (
  IN OC_INTERFACE_PROTOCOL    *This,
  IN OC_STORAGE_CONTEXT       *Storage,
  IN OC_PICKER_CONTEXT        *Context
  )
{
  mStorage = Storage;

  Context->ShowMenu = OcShowMenuByOc;

  return EFI_SUCCESS;
}

STATIC
OC_INTERFACE_PROTOCOL
mOcInterface = {
  OC_INTERFACE_REVISION,
  GuiOcInterfacePopulate
};

EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS    Status;
  VOID          *PrevInterface;
  EFI_HANDLE    NewHandle;

  //
  // Check for previous GUI protocols.
  //
  Status = gBS->LocateProtocol (
    &gOcInterfaceProtocolGuid,
    NULL,
    &PrevInterface
    );

  if (!EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "OCUI: Another GUI is already present\n"));
    return EFI_ALREADY_STARTED;
  }

  //
  // Install new GUI protocol
  //
  NewHandle = NULL;
  Status = gBS->InstallMultipleProtocolInterfaces (
    &NewHandle,
    &gOcInterfaceProtocolGuid,
    &mOcInterface,
    NULL
    );

  if (!EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "OCUI: Registered custom GUI protocol\n"));
  } else {
    DEBUG ((DEBUG_WARN, "OCUI: Failed to install GUI protocol - %r\n", Status));
  }

  return Status;
}
