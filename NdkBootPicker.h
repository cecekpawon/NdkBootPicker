//
//  NdkBootPicker.h
//
//
//  Created by N-D-K on 1/24/20.
//

#ifndef NdkBootPicker_h
#define NdkBootPicker_h

#include <Guid/AppleVariable.h>

#include <Protocol/GraphicsOutput.h>
#include <Protocol/SimpleTextOut.h>
#include <Protocol/UgaDraw.h>
#include <Protocol/OcInterface.h>
#include <Protocol/AppleKeyMapAggregator.h>
#include <Protocol/SimplePointer.h>

#include <IndustryStandard/AppleCsrConfig.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/OcDebugLogLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/OcAppleKeyMapLib.h>
#include <Library/OcBootManagementLib.h>
#include <Library/OcConsoleLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/OcPngLib.h>
#include <Library/OcFileLib.h>
#include <Library/OcStorageLib.h>
#include <Library/OcMiscLib.h>
#include <Library/OcTimerLib.h>

#define NDK_BOOTPICKER_VERSION   "0.1.9"

/*========== UI's defined variables ==========*/

#define UI_IMAGE_DIR                  L"Icons\\"

#define UI_IMAGE_POINTER              UI_IMAGE_DIR L"pointer4k.png"
#define UI_IMAGE_POINTER_ALT          UI_IMAGE_DIR L"pointer.png"
#define UI_IMAGE_POINTER_HAND         UI_IMAGE_DIR L"pointeralt.png"
#define UI_IMAGE_FONT                 UI_IMAGE_DIR L"font.png"
#define UI_IMAGE_FONT_COLOR           UI_IMAGE_DIR L"font_color.png"
#define UI_IMAGE_BACKGROUND           UI_IMAGE_DIR L"background4k.png"
#define UI_IMAGE_BACKGROUND_ALT       UI_IMAGE_DIR L"background.png"
#define UI_IMAGE_BACKGROUND_COLOR     UI_IMAGE_DIR L"background_color.png"
#define UI_IMAGE_SELECTOR             UI_IMAGE_DIR L"selector4k.png"
#define UI_IMAGE_SELECTOR_ALT         UI_IMAGE_DIR L"selector.png"
#define UI_IMAGE_SELECTOR_OFF         UI_IMAGE_DIR L"no_selector.png"
#define UI_IMAGE_SELECTOR_FUNC        UI_IMAGE_DIR L"func_selector.png"
#define UI_IMAGE_LABEL                UI_IMAGE_DIR L"label.png"
#define UI_IMAGE_LABEL_OFF            UI_IMAGE_DIR L"no_label.png"
#define UI_IMAGE_TEXT_SCALE_OFF       UI_IMAGE_DIR L"No_text_scaling.png"
#define UI_IMAGE_ICON_SCALE_OFF       UI_IMAGE_DIR L"No_icon_scaling.png"


#define UI_ICON_WIN                   UI_IMAGE_DIR L"os_win.icns"
#define UI_ICON_WIN10                 UI_IMAGE_DIR L"os_win10.icns"
#define UI_ICON_INSTALL               UI_IMAGE_DIR L"os_install.icns"
#define UI_ICON_MAC                   UI_IMAGE_DIR L"os_mac.icns"
#define UI_ICON_MAC_CATA              UI_IMAGE_DIR L"os_cata.icns"
#define UI_ICON_MAC_MOJA              UI_IMAGE_DIR L"os_moja.icns"
#define UI_ICON_MAC_RECOVERY          UI_IMAGE_DIR L"os_recovery.icns"
#define UI_ICON_CLONE                 UI_IMAGE_DIR L"os_clone.icns"
#define UI_ICON_FREEBSD               UI_IMAGE_DIR L"os_freebsd.icns"
#define UI_ICON_LINUX                 UI_IMAGE_DIR L"os_linux.icns"
#define UI_ICON_REDHAT                UI_IMAGE_DIR L"os_redhat.icns"
#define UI_ICON_UBUNTU                UI_IMAGE_DIR L"os_ubuntu.icns"
#define UI_ICON_FEDORA                UI_IMAGE_DIR L"os_fedora.icns"
#define UI_ICON_DEBIAN                UI_IMAGE_DIR L"os_debian.icns"
#define UI_ICON_ARCH                  UI_IMAGE_DIR L"os_arch.icns"
#define UI_ICON_UBUNTU                UI_IMAGE_DIR L"os_ubuntu.icns"
#define UI_ICON_CUSTOM                UI_IMAGE_DIR L"os_custom.icns"
#define UI_ICON_SHELL                 UI_IMAGE_DIR L"tool_shell.icns"
#define UI_ICON_RESETNVRAM            UI_IMAGE_DIR L"func_resetnvram.icns"
#define UI_ICON_RESET                 UI_IMAGE_DIR L"func_reset.icns"
#define UI_ICON_SHUTDOWN              UI_IMAGE_DIR L"func_shutdown.icns"
#define UI_ICON_UNKNOWN               UI_IMAGE_DIR L"os_unknown.icns"

#define UI_MENU_SYSTEM_RESET          L"Restart"
#define UI_MENU_SYSTEM_SHUTDOWN       L"Shutdown"
#define UI_MENU_POINTER_SPEED         L"PointerSpeed"
#define UI_INPUT_SYSTEM_RESET         99
#define UI_INPUT_SYSTEM_SHUTDOWN      100

/*========== Image ==========*/

typedef
VOID
(*NDK_ICON_ACTION)(
  IN EFI_RESET_TYPE   ResetType
  );

typedef struct _NDK_UI_IMAGE {
  UINT16                          Width;
  UINT16                          Height;
  BOOLEAN                         IsAlpha;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL   *Bitmap;
} NDK_UI_IMAGE;

typedef struct _NDK_UI_ICON {
  INTN              Xpos;
  INTN              Ypos;
  BOOLEAN           IsSelected;
  NDK_ICON_ACTION   Action;
  NDK_UI_IMAGE      *Image;
  NDK_UI_IMAGE      *Selector;
} NDK_UI_ICON;

/*========== Pointer ==========*/

#define POINTER_WIDTH  32
#define POINTER_HEIGHT 32

#define OC_INPUT_POINTER  -50       ///< Pointer left click
#define OC_INPUT_MENU     -51       ///<Tab back to menu entries
#define OC_INPUT_TAB      -52       ///<Tab away from menu entries

typedef enum {
  NoEvents,
  Move,
  LeftClick,
  RightClick,
  DoubleClick,
  ScrollClick,
  ScrollDown,
  ScrollUp,
  LeftMouseDown,
  RightMouseDown,
  MouseMove
} MOUSE_EVENT;

typedef struct {
  INTN     Xpos;
  INTN     Ypos;
  INTN     Width;
  INTN     Height;
} AREA_RECT;

typedef struct _pointers {
  EFI_SIMPLE_POINTER_PROTOCOL   *SimplePointerProtocol;
  NDK_UI_IMAGE                  *Pointer;
  NDK_UI_IMAGE                  *PointerAlt;
  NDK_UI_IMAGE                  *NewImage;
  NDK_UI_IMAGE                  *OldImage;

  AREA_RECT                     NewPlace;
  AREA_RECT                     OldPlace;

  BOOLEAN                       IsClickable;
  UINT64                        LastClickTime;
  EFI_SIMPLE_POINTER_STATE      State;
  MOUSE_EVENT                   MouseEvent;
} POINTERS;

/*================ ImageSupport.c =============*/

#define ICON_BRIGHTNESS_LEVEL   80
#define ICON_BRIGHTNESS_FULL    0
#define ICON_ROW_SPACE_OFFSET   20

NDK_UI_IMAGE *
CreateImage (
  IN UINT16     Width,
  IN UINT16     Height,
  IN BOOLEAN    IsAlpha
  );

NDK_UI_IMAGE *
CreateFilledImage (
  IN INTN                             Width,
  IN INTN                             Height,
  IN BOOLEAN                          IsAlpha,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL    *Color
  );

NDK_UI_IMAGE *
CopyImage (
  IN NDK_UI_IMAGE   *Image
  );

NDK_UI_IMAGE *
CopyScaledImage (
  IN NDK_UI_IMAGE   *OldImage,
  IN INTN           Ratio
  );

NDK_UI_IMAGE *
DecodePNG (
  IN VOID     *Buffer,
  IN UINT32   BufferSize
  );

VOID
BltImage (
  IN NDK_UI_IMAGE   *Image,
  IN INTN           Xpos,
  IN INTN           Ypos
  );

VOID
RestrictImageArea (
  IN     NDK_UI_IMAGE   *Image,
  IN     INTN           AreaXpos,
  IN     INTN           AreaYpos,
  IN OUT INTN           *AreaWidth,
  IN OUT INTN           *AreaHeight
  );

VOID
ComposeImage (
  IN OUT NDK_UI_IMAGE   *Image,
  IN     NDK_UI_IMAGE   *TopImage,
  IN     INTN           Xpos,
  IN     INTN           Ypos
  );

VOID
RawCompose (
  IN OUT EFI_GRAPHICS_OUTPUT_BLT_PIXEL    *CompBasePtr,
  IN     EFI_GRAPHICS_OUTPUT_BLT_PIXEL    *TopBasePtr,
  IN     INTN                             Width,
  IN     INTN                             Height,
  IN     INTN                             CompLineOffset,
  IN     INTN                             TopLineOffset
  );

VOID
RawComposeOnFlat (
  IN OUT EFI_GRAPHICS_OUTPUT_BLT_PIXEL    *CompBasePtr,
  IN     EFI_GRAPHICS_OUTPUT_BLT_PIXEL    *TopBasePtr,
  IN     INTN                             Width,
  IN     INTN                             Height,
  IN     INTN                             CompLineOffset,
  IN     INTN                             TopLineOffset
  );

VOID
RawCopy (
  IN OUT EFI_GRAPHICS_OUTPUT_BLT_PIXEL    *CompBasePtr,
  IN     EFI_GRAPHICS_OUTPUT_BLT_PIXEL    *TopBasePtr,
  IN     INTN                             Width,
  IN     INTN                             Height,
  IN     INTN                             CompLineOffset,
  IN     INTN                             TopLineOffset
  );

//
// Opacity level can be set from 1-255, 0 = Off
//
//VOID
//RawComposeAlpha (
//  IN OUT EFI_GRAPHICS_OUTPUT_BLT_PIXEL    *CompBasePtr,
//  IN     EFI_GRAPHICS_OUTPUT_BLT_PIXEL    *TopBasePtr,
//  IN     INTN                             Width,
//  IN     INTN                             Height,
//  IN     INTN                             CompLineOffset,
//  IN     INTN                             TopLineOffset,
//  IN     INTN                             Opacity
//  );

//
// ColorDiff is adjustable color saturation level to Top Image which can be set from -255 to 255, 0 = no adjustment.
//
VOID
RawComposeColor (
  IN OUT EFI_GRAPHICS_OUTPUT_BLT_PIXEL    *CompBasePtr,
  IN     EFI_GRAPHICS_OUTPUT_BLT_PIXEL    *TopBasePtr,
  IN     INTN                             Width,
  IN     INTN                             Height,
  IN     INTN                             CompLineOffset,
  IN     INTN                             TopLineOffset,
  IN     INTN                             ColorDiff
  );

VOID
FillImage (
  IN OUT NDK_UI_IMAGE                     *Image,
  IN     EFI_GRAPHICS_OUTPUT_BLT_PIXEL    *Color
  );

VOID
FreeImage (
  IN NDK_UI_IMAGE   *Image
  );

/*======= NdkBootPicker.c =========*/

VOID
DrawImageArea (
  IN NDK_UI_IMAGE   *Image,
  IN INTN           AreaXpos,
  IN INTN           AreaYpos,
  IN INTN           AreaWidth,
  IN INTN           AreaHeight,
  IN INTN           ScreenXpos,
  IN INTN           ScreenYpos
  );

#endif /* NdkBootPicker_h */
