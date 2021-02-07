//
//  AudioSupport.c
//
//
//  @cecekpawon - Feb 2021
//

#include <NdkBootPicker.h>

STATIC EFI_AUDIO_DECODE_PROTOCOL    *mAudioDecodeProtocol;
STATIC CHAR16                       *mAudioDbFilename[AudioIndexInvalid]  = { L"AudioHover", L"AudioToolbarHover", L"AudioSelect" };
STATIC AUDIO_DB                     mAudioDb[AudioIndexInvalid];
STATIC BOOLEAN                      mAudioPlaybackComplete                = FALSE;
STATIC EFI_AUDIO_IO_PROTOCOL        *mAudioIo                             = NULL;
STATIC EFI_DEVICE_PATH_PROTOCOL     *mAudioDevicePath                     = NULL;
STATIC UINT8                        mAudioDeviceVolume                    = OC_AUDIO_DEFAULT_VOLUME_LEVEL;
STATIC UINT8                        mAudioOutputPortIndex                 = 0;
STATIC BOOLEAN                      mAudioDisable                         = TRUE;

STATIC
EFI_DEVICE_PATH_PROTOCOL *
GetRootDevicePath (
  IN  EFI_DEVICE_PATH_PROTOCOL    *DevicePath
  )
{
  EFI_DEVICE_PATH_PROTOCOL    *TmpDevicePath;
  VENDOR_DEVICE_PATH          *VendorDevicePath;

  //

  if (DevicePath != NULL) {
    TmpDevicePath = DuplicateDevicePath (DevicePath);
    if (TmpDevicePath != NULL) {
      VendorDevicePath  = (VENDOR_DEVICE_PATH *)FindDevicePathNodeWithType (
                                                  TmpDevicePath,
                                                  MESSAGING_DEVICE_PATH,
                                                  MSG_VENDOR_DP);
      if (VendorDevicePath != NULL) {
        SetDevicePathEndNode (VendorDevicePath);
        return TmpDevicePath;
      }

      FreePool (TmpDevicePath);
    }
  }

  return NULL;
}

STATIC
EFI_STATUS
GetOutputDevices (
  VOID
  )
{
  EFI_STATUS                    Status;
  EFI_HANDLE                    *AudioIoHandles;
  UINTN                         AudioIoHandleCount;
  EFI_AUDIO_IO_PROTOCOL         *AudioIo;
  EFI_DEVICE_PATH_PROTOCOL      *DevicePath;
  EFI_AUDIO_IO_PROTOCOL_PORT    *OutputPorts;
  UINTN                         OutputPortsCount;
  UINTN                         h;
  UINTN                         o;
  BOOLEAN                       Equal;
  EFI_DEVICE_PATH_PROTOCOL      *TmpDevicePath;

  //

  if (mAudioDisable) {
    return EFI_UNSUPPORTED;
  }

  // Get Audio I/O protocols in system.
  AudioIoHandles      = NULL;
  AudioIoHandleCount  = 0;
  Status              = gBS->LocateHandleBuffer (ByProtocol, &gEfiAudioIoProtocolGuid, NULL, &AudioIoHandleCount, &AudioIoHandles);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //DEBUG ((DEBUG_INFO, "AudioIoHandleCount (%d)\n", AudioIoHandleCount));

  // Discover audio outputs in system.
  for (h = 0; h < AudioIoHandleCount; h++) {
    // Open Audio I/O protocol.
    Status = gBS->HandleProtocol (AudioIoHandles[h], &gEfiAudioIoProtocolGuid, (VOID**)&AudioIo);
    if (EFI_ERROR (Status)) {
      continue;
    }

    // Get device path.
    Status = gBS->HandleProtocol (AudioIoHandles[h], &gEfiDevicePathProtocolGuid, (VOID**)&DevicePath);
    if (EFI_ERROR (Status)) {
      continue;
    }

    // Auto select 1st found output if no mAudioDevicePath.
    if (mAudioDevicePath == NULL) {
      Equal = TRUE;
    } else {
      TmpDevicePath = GetRootDevicePath (DevicePath);
      if (TmpDevicePath == NULL) {
        continue;
      }
      Equal = IsDevicePathEqual (TmpDevicePath, mAudioDevicePath);
      FreePool (TmpDevicePath);
    }
    //DEBUG ((DEBUG_INFO, "Equal (%d)\n", Equal));
    if (!Equal) {
      continue;
    }

    // Get output devices.
    Status = AudioIo->GetOutputs (AudioIo, &OutputPorts, &OutputPortsCount);
    if (EFI_ERROR (Status)) {
      continue;
    }

    // Auto select 1st found port if invalid mAudioOutputPortIndex.
    if (OutputPortsCount > 0 && (UINTN)mAudioOutputPortIndex >= OutputPortsCount) {
      mAudioOutputPortIndex = 0;
      mAudioIo              = AudioIo;
      break;
    } else {
      // Get devices on this protocol.
      for (o = 0; o < OutputPortsCount; o++) {
        //DEBUG ((DEBUG_INFO, "OutputPort %d <--> %d\n", o, mAudioOutputPortIndex));
        if (o == (UINTN)mAudioOutputPortIndex) {
          mAudioIo = AudioIo;
          break;
        }
      }
    }

    // Free output ports.
    FreePool (OutputPorts);

    if (mAudioIo != NULL) {
      //DEBUG ((DEBUG_INFO, "Got mAudioIo\n"));
      break;
    }
  }

  // Free stuff.
  if (AudioIoHandles != NULL) {
    FreePool (AudioIoHandles);
  }

  if (mAudioIo == NULL) {
    mAudioDisable = TRUE;
  }

  return Status;
}

STATIC
VOID
EFIAPI
PlaybackAsyncCallback (
  IN  EFI_AUDIO_IO_PROTOCOL   *AudioIo,
  IN  VOID                    *Context
  )
{
  mAudioPlaybackComplete  = TRUE;
  *((BOOLEAN *)Context)   = mAudioPlaybackComplete;
}

EFI_STATUS
PlayAudio (
  IN  AUDIO_INDEX   Index
  )
{
  EFI_STATUS    Status;

  //

  Status = EFI_UNSUPPORTED;

  if (mAudioDisable) {
    return Status;
  }

  if (!mAudioPlaybackComplete) {
    Status = mAudioIo->StopPlayback (mAudioIo);
  }

  mAudioPlaybackComplete = FALSE;

  // Try to use Index 0 if Index is invalid / choosen Index has invalid buffer / bad decoded.
  if (Index >= AudioIndexInvalid || mAudioDb[Index].OutBuffer == NULL) {
    Index = AudioIndexHover;
  }

  if (mAudioDb[Index].OutBuffer != NULL) {
    //DEBUG ((DEBUG_INFO, "mAudioOutputPortIndex (%d) Vol (%d)\n", mAudioOutputPortIndex, mAudioDeviceVolume));
    Status = mAudioIo->SetupPlayback (
                        mAudioIo,
                        (UINT8)mAudioOutputPortIndex,
                        mAudioDeviceVolume,
                        mAudioDb[Index].Frequency,
                        mAudioDb[Index].Bits,
                        mAudioDb[Index].Channels);
    //DEBUG ((DEBUG_INFO, "SetupPlayback - %r\n", Status));
    if (!EFI_ERROR (Status)) {
      Status = mAudioIo->StartPlaybackAsync (mAudioIo, mAudioDb[Index].OutBuffer, mAudioDb[Index].OutBufferSize, 0, PlaybackAsyncCallback, NULL);
      //DEBUG ((DEBUG_INFO, "StartPlayback - %r\n", Status));
    }
  }

  return Status;
}

STATIC
EFI_STATUS
GetAudioDecoder (
  IN  OC_STORAGE_CONTEXT  *Storage,
  IN  AUDIO_DB            *AudioDb
  )
{
  EFI_STATUS    Status;
  CHAR16        Path[OC_STORAGE_SAFE_PATH_MAX];

  //

  if (mAudioDisable) {
    return EFI_UNSUPPORTED;
  }

  if (mAudioDecodeProtocol == NULL) {
    Status = gBS->LocateProtocol (
      &gEfiAudioDecodeProtocolGuid,
      NULL,
      (VOID **)&mAudioDecodeProtocol
      );
  } else {
    Status = EFI_SUCCESS;
  }
  if (!EFI_ERROR (Status)) {
    Status = OcUnicodeSafeSPrint (
      Path,
      sizeof (Path),
      UI_IMAGE_DIR L"%s.mp3",
      mAudioDbFilename[AudioDb->AudioIndex]
      );
    if (EFI_ERROR (Status)) {
      return EFI_OUT_OF_RESOURCES;
    }
    //DEBUG ((DEBUG_INFO, "Path: %s\n", Path));

    if (OcStorageExistsFileUnicode (Storage, Path)) {
      AudioDb->InBuffer = OcStorageReadFileUnicode (Storage, Path, &AudioDb->InBufferSize);
      //DEBUG ((DEBUG_INFO, "InBuffer (%c) InBufferSize (%d)\n", AudioDb->InBuffer != NULL ? 'Y' : 'N', AudioDb->InBufferSize));
      if (AudioDb->InBuffer != NULL && AudioDb->InBufferSize > 0) {
        Status = mAudioDecodeProtocol->DecodeAny (
          mAudioDecodeProtocol,
          AudioDb->InBuffer,
          AudioDb->InBufferSize,
          (VOID **)&AudioDb->OutBuffer,
          &AudioDb->OutBufferSize,
          &AudioDb->Frequency,
          &AudioDb->Bits,
          &AudioDb->Channels
          );
        //DEBUG ((DEBUG_INFO, "DecodeAny - %r\n", Status));
        //DEBUG ((DEBUG_INFO, "OutBuffer (%c) OutBufferSize (%d)\n", AudioDb->OutBuffer != NULL ? 'Y' : 'N', AudioDb->OutBufferSize));
        //DEBUG ((DEBUG_INFO, "Frequency (%d) Bits (%d) Channels (%d) \n", AudioDb->Frequency, AudioDb->Bits, AudioDb->Channels));
      } else {
        //DEBUG ((DEBUG_WARN, "BAD read file\n"));
      }
    } else {
      //DEBUG ((DEBUG_WARN, "Path NOT Exists\n"));
    }
  } else {
    //DEBUG ((DEBUG_WARN, "NO mAudioDecodeProtocol\n"));
  }

  return Status;
}

VOID
GetAudioConfig (
  IN OC_STORAGE_CONTEXT   *Storage
  )
{
  EFI_STATUS          Status;
  CHAR8               *ConfigData;
  UINT32              ConfigDataSize;
  OC_GLOBAL_CONFIG    Config;
  CHAR8               *AsciiDevicePath;
  CHAR16              *UnicodeDevicePath;
  BOOLEAN             Muted;

  //

  Muted = FALSE;

  //
  // 2/7/2021 2:01:30 PM
  // I think this driver should have very own config (like OcQuirks?) in the future to get more wild.
  // To get user defined pointer speed value, custom Icons path, choosen theme, etc, including this audio setting.
  // For now, the current settings that we need is pretty much same as OC config.
  //

  // Read Main OC config.
  ConfigData = OcStorageReadFileUnicode (
    Storage,
    OPEN_CORE_CONFIG_PATH,
    &ConfigDataSize
    );

  if (ConfigData == NULL) {
    // Read other (fake) config from Icons dir ('\Icons\config.plist').
    // The only need to read is only Uefi->Audio section for now.
    ConfigData = OcStorageReadFileUnicode (
      Storage,
      UI_IMAGE_DIR  OPEN_CORE_CONFIG_PATH,
      &ConfigDataSize
      );
  }

  if (ConfigData != NULL) {
    //DEBUG ((DEBUG_INFO, "OC: Loaded configuration of %u bytes\n", ConfigDataSize));

    Status = OcConfigurationInit (&Config, ConfigData, ConfigDataSize, NULL);
    //DEBUG ((DEBUG_INFO, "OcConfigurationInit - %r\n", Status));
    // FIXME: Dont care whether AudioSupport is set to enabled or not, since user may load AudioDxe by hand?
    if (!EFI_ERROR (Status) /*&& Config.Uefi.Audio.AudioSupport*/) {
      mAudioDeviceVolume  = (UINT8)Config.Uefi.Audio.VolumeAmplifier;
      if (mAudioDeviceVolume > 0) {
        if (mAudioDeviceVolume > EFI_AUDIO_IO_PROTOCOL_MAX_VOLUME) {
          mAudioDeviceVolume = EFI_AUDIO_IO_PROTOCOL_MAX_VOLUME;
        }
        if (mAudioDeviceVolume < Config.Uefi.Audio.MinimumVolume) {
          mAudioDeviceVolume = Config.Uefi.Audio.MinimumVolume;
        }
      }

      if (mAudioDeviceVolume == 0) {
        Muted = TRUE;
      }

      //DEBUG ((DEBUG_INFO, "mAudioDeviceVolume (%d)\n", mAudioDeviceVolume));
      //DEBUG ((DEBUG_INFO, "Muted (%d)\n", Muted));

      if (!Muted) {
        mAudioDeviceVolume = OcGetVolumeLevel (mAudioDeviceVolume, &Muted);
        //DEBUG ((DEBUG_INFO, "mAudioDeviceVolume (%d)\n", mAudioDeviceVolume));
        //DEBUG ((DEBUG_INFO, "Muted (%d)\n", Muted));
        if (!Muted) {
          AsciiDevicePath = OC_BLOB_GET (&Config.Uefi.Audio.AudioDevice);
          if (AsciiDevicePath[0] != '\0') {
            UnicodeDevicePath = AsciiStrCopyToUnicode (AsciiDevicePath, 0);
            if (UnicodeDevicePath != NULL) {
              mAudioDevicePath = ConvertTextToDevicePath (UnicodeDevicePath);
              //DEBUG ((DEBUG_INFO, "UnicodeDevicePath (%s)\n", UnicodeDevicePath));
              FreePool (UnicodeDevicePath);
            }
          }

          mAudioOutputPortIndex = Config.Uefi.Audio.AudioOut;
          //DEBUG ((DEBUG_INFO, "mAudioOutputPortIndex (%d)\n", mAudioOutputPortIndex));
        }
      }

      OcConfigurationFree (&Config);
    } else {
      //DEBUG ((DEBUG_WARN, "OC: Failed to parse configuration!\n"));
    }

    FreePool (ConfigData);
  } else {
    //DEBUG ((DEBUG_WARN, "OC: Failed to load configuration!\n"));
  }

  // Set to unsupported state if Muted.
  mAudioDisable = Muted;
}

VOID
InitAudioResources (
  IN  OC_STORAGE_CONTEXT  *Storage
  )
{
  //EFI_STATUS    Status;
  UINTN         Index;
  UINTN         Count;

  //

  if (mAudioDisable) {
    return;
  }

  GetOutputDevices ();

  if (mAudioDisable) {
    return;
  }

  Count = AudioIndexInvalid;

  for (Index = 0; Index < Count; ++Index) {
    ZeroMem (&mAudioDb[Index], sizeof (mAudioDb[Index]));
    mAudioDb[Index].AudioIndex = Index;
    /*Status =*/ GetAudioDecoder (Storage, &mAudioDb[Index]);
    //DEBUG ((DEBUG_INFO, "GetAudioDecoder - %r\n", Status));
    //DEBUG ((DEBUG_INFO, "%d OutBuffer '%c' (%d)\n", Index, mAudioDb[Index].OutBuffer != NULL ? 'Y' : 'N', mAudioDb[Index].OutBufferSize));
  }
}

VOID
FreeAudioResources (
  VOID
  )
{
  UINTN   Index;
  UINTN   Count;

  //

  if (mAudioDisable) {
    return;
  }

  Count = AudioIndexInvalid;

  for (Index = 0; Index < Count; ++Index) {
    if (mAudioDb[Index].InBuffer != NULL) {
      FreePool (mAudioDb[Index].InBuffer);
    }
    if (mAudioDb[Index].OutBuffer != NULL) {
      FreePool (mAudioDb[Index].OutBuffer);
    }
  }

  if (mAudioDevicePath != NULL) {
    FreePool (mAudioDevicePath);
  }
}
