#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/Http.h>
#include <Protocol/ServiceBinding.h>
#include <Library/BaseMemoryLib.h>
#include <Library/JsonLib.h>
//#include <Protocol/LoadedImage.h>

#define BUFFER_SIZE 0x010000

BOOLEAN gRequestCallbackComplete = FALSE;
BOOLEAN gResponseCallbackComplete = FALSE;

VOID EFIAPI RequestCallback(IN EFI_EVENT event, IN VOID *context)
{
    gRequestCallbackComplete = TRUE;
}

VOID EFIAPI ResponseCallback(IN EFI_EVENT event, IN VOID *context)
{
    gResponseCallbackComplete = TRUE;
}

EFI_STATUS EFIAPI EvaluateKeyResponse()
{
    EFI_STATUS status;
    UINTN index;
    EFI_EVENT waitList[1];
    EFI_INPUT_KEY inputKey;

    // Suppress unused error
    (void)(status);
    
    waitList[0] = gST->ConIn->WaitForKey;

    Print(L"Press any key to continue...\n");
    Print(L"Press F1 to shutdown, F2 to restart...\n");

    gST->ConIn->Reset(gST->ConIn, FALSE);
    status = gST->BootServices->WaitForEvent(1, waitList, &index);

    gST->ConIn->ReadKeyStroke(gST->ConIn, &inputKey);
    switch(inputKey.ScanCode)
    {
        case SCAN_F1:
            gST->RuntimeServices->ResetSystem(EfiResetShutdown, EFI_SUCCESS, 0, NULL);
            CpuDeadLoop();
            break;
        
        case SCAN_F2:
            gST->RuntimeServices->ResetSystem(EfiResetWarm, EFI_SUCCESS, 0, NULL);
            CpuDeadLoop();
            break;
            
        default:
            break;
    }

    return EFI_SUCCESS;
}

VOID EFIAPI AppendUINT8ToUINT8(IN UINT8 *dest, IN UINT8 *src, IN UINTN length, IN UINTN continuePoint)
{
    UINTN newLength = continuePoint + length;
    UINTN srcPoint = 0;
    for(int i = continuePoint; i < newLength; i++)
    {
        dest[i] = src[srcPoint];
        srcPoint++;
    }
}

EFI_STATUS EFIAPI efi_main(IN EFI_HANDLE imgHandle, IN EFI_SYSTEM_TABLE *sysTable)
{
    CHAR16 *testUrl;
    EFI_STATUS status;
    EFI_HTTP_CONFIG_DATA configData;
    EFI_HTTP_REQUEST_DATA requestData;
    EFI_HTTPv4_ACCESS_POINT IPv4Node;
    EFI_HTTP_PROTOCOL *HttpProtocol;
    EFI_HTTP_MESSAGE requestMessage;
    EFI_HTTP_TOKEN requestToken;
    EFI_HTTP_HEADER requestHeader[3];
    EFI_HTTP_RESPONSE_DATA responseData;
    EFI_HTTP_MESSAGE responseMessage;
    EFI_HTTP_TOKEN responseToken;
    UINTN timer;
    UINT8 *buffer;
    EFI_SERVICE_BINDING_PROTOCOL *serviceBinding;
    EFI_HANDLE handle;
    EDKII_JSON_OBJECT *vmList;
    EDKII_JSON_ERROR *jsonError;
    UINTN contentLength;
    UINTN contentDownloaded;
    //EFI_LOADED_IMAGE_PROTOCOL *loadedImage;
    //EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fileSystem;
    //EFI_FILE *root;
    //EFI_FILE *logFile;
    //EFI_GUID gEfiLoadedImageProtocolGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;

    // Suppress unused error
    (void)(status);

    testUrl = L"http://10.0.1.8:18964/api/get_vms";
    handle = NULL;
    contentLength = BUFFER_SIZE;

    gST = sysTable;
    gBS = sysTable->BootServices;
    gImageHandle = imgHandle;
    // UEFI apps automatically exit after 5 minutes. Stop that here
    gBS->SetWatchdogTimer(0, 0, 0, NULL);

    gST->ConOut->ClearScreen(gST->ConOut);

    // Wait 5 seconds for network to init
    Print(L"Waiting for network to init...\n");
    gBS->Stall(5000000);

    status = gBS->AllocatePool(EfiBootServicesData, BUFFER_SIZE, (VOID **)&buffer);
    status = gBS->LocateProtocol(&gEfiHttpServiceBindingProtocolGuid, NULL, (VOID **)&serviceBinding);

    if(status != EFI_SUCCESS)
    {
        Print(L"Http driver is not available!\n");
        //Print(L"Http driver is not available! (%r)\n", status);
        return EvaluateKeyResponse();
    }

    status = serviceBinding->CreateChild(serviceBinding, &handle);
    status = gBS->HandleProtocol(handle, &gEfiHttpProtocolGuid, (VOID **)&HttpProtocol);

    //status = gBS->HandleProtocol(imgHandle, &gEfiLoadedImageProtocolGuid, (VOID **)&loadedImage);
    //status = gBS->HandleProtocol(loadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (VOID **)&fileSystem);

    configData.HttpVersion = HttpVersion11;
    configData.TimeOutMillisec = 0;
    configData.LocalAddressIsIPv6 = FALSE;
    ZeroMem(&IPv4Node, sizeof(IPv4Node));
    IPv4Node.UseDefaultAddress = TRUE;
    configData.AccessPoint.IPv4Node = &IPv4Node;
    status = HttpProtocol->Configure(HttpProtocol, &configData);
    
    requestData.Method = HttpMethodGet;
    requestData.Url = testUrl;
    requestMessage.Data.Request = &requestData;
    requestMessage.HeaderCount = 3;

    requestHeader[0].FieldName = "Host";
    requestHeader[0].FieldValue = "10.0.1.8:18964";
    requestHeader[1].FieldName = "Connection";
    requestHeader[1].FieldValue = "close";
    requestHeader[2].FieldName = "User-Agent";
    requestHeader[2].FieldValue = "Mozilla/5.0 (EDK2; Linux) Gecko/20100101 Firefox/79.0";

    requestMessage.Headers = requestHeader;
    requestMessage.BodyLength = 0;
    requestMessage.Body = NULL;
    requestToken.Event = NULL;
    status = gBS->CreateEvent(EVT_NOTIFY_SIGNAL, TPL_CALLBACK, RequestCallback, NULL, &requestToken.Event);
    requestToken.Status = EFI_SUCCESS;
    requestToken.Message = &requestMessage;
    gRequestCallbackComplete = FALSE;
    // Actual HTTP request...
    status = HttpProtocol->Request(HttpProtocol, &requestToken);
    for(timer = 0; !gRequestCallbackComplete && timer < 10;)
    {
        HttpProtocol->Poll(HttpProtocol);
        gBS->Stall(1000000);
        Print(L".");
        timer++;
    }

    if(!gRequestCallbackComplete)
    {
        status = HttpProtocol->Cancel(HttpProtocol, &requestToken);
        Print(L"Server did not respond in time!\r\n");
        return EvaluateKeyResponse();
    }

    responseData.StatusCode = HTTP_STATUS_UNSUPPORTED_STATUS;
    responseMessage.Data.Response = &responseData;
    responseMessage.HeaderCount = 0;
    responseMessage.Headers = NULL;
    responseMessage.BodyLength = BUFFER_SIZE;
    responseMessage.Body = buffer;
    status = gBS->CreateEvent(EVT_NOTIFY_SIGNAL, TPL_CALLBACK, ResponseCallback, &responseToken, &responseToken.Event);
    responseToken.Status = EFI_SUCCESS;
    responseToken.Message = &responseMessage;
    gResponseCallbackComplete = FALSE;
    status = HttpProtocol->Response(HttpProtocol, &responseToken);
    for(timer = 0; !gResponseCallbackComplete && timer < 10;)
    {
        HttpProtocol->Poll(HttpProtocol);
        gBS->Stall(500000);
        Print(L".");
        timer++;
    }

    if(!gResponseCallbackComplete)
    {
        status = HttpProtocol->Cancel(HttpProtocol, &responseToken);
        Print(L"Unknown error occurred receiving data!\r\n");
        return EvaluateKeyResponse();
    }

    // Get length of body
    for (int i = 0; i < responseMessage.HeaderCount; i++)
    {
        if (!AsciiStriCmp(responseMessage.Headers[i].FieldName, "Content-Length")) {
            contentLength = AsciiStrDecimalToUintn(responseMessage.Headers[i].FieldValue);
        }
    }

    // Right now if the total response body size is greaten than approx 4100, the http driver returns junk

    contentDownloaded = responseMessage.BodyLength;
    UINT8 *fullBuffer;
    status = gBS->AllocatePool(EfiBootServicesData, BUFFER_SIZE, (VOID **)&fullBuffer);
    CopyMem(fullBuffer, buffer, contentDownloaded);

    while(contentDownloaded < contentLength)
    {
        responseMessage.Data.Response = NULL;
        responseMessage.HeaderCount = 0;
        responseMessage.Headers = NULL;
        responseMessage.BodyLength = BUFFER_SIZE;

        ZeroMem(buffer, BUFFER_SIZE);

        gResponseCallbackComplete = FALSE;

        status = HttpProtocol->Response(HttpProtocol, &responseToken);
        for(timer = 0; !gResponseCallbackComplete && timer < 10;)
        {
            HttpProtocol->Poll(HttpProtocol);
            gBS->Stall(500000);
            Print(L".");
            timer++;
        }

        if (!gResponseCallbackComplete)
        {
            status = HttpProtocol->Cancel(HttpProtocol, &responseToken);
            Print(L"Unknown error occurred receiving data!\r\n");
            return EvaluateKeyResponse();
        }

        UINT8 *tempBuffer;
        CopyMem(tempBuffer, buffer, responseMessage.BodyLength);
        AppendUINT8ToUINT8(fullBuffer, tempBuffer, responseMessage.BodyLength, contentDownloaded);

        contentDownloaded += responseMessage.BodyLength;
    }

    Print(L"\n");

    // Copy and convert uint8 to char8 while retaining the null terminator (\0)
    UINT8 *bufferCpy;
    CHAR8 *bufferChar8;
    CopyMem(bufferCpy, fullBuffer, contentLength + 1);
    status = gBS->AllocatePool(EfiBootServicesData, contentLength + 1, (VOID **)&bufferChar8);
    ZeroMem(bufferChar8, contentLength + 1);
    for(int i = 0; i < contentLength; i++)
    {
        bufferChar8[i] = (CHAR8)bufferCpy[i];
    }

    vmList = JsonLoadString(bufferChar8, 0, jsonError);
    if(vmList == NULL){
        //Print(L"1\n");
        //status = fileSystem->OpenVolume(fileSystem, &root);
        //Print(L"2\n");
        //status = root->Open(root, &logFile, L"log.txt", EFI_FILE_MODE_WRITE, EFI_FILE_MODE_WRITE);
        //Print(L"3\n");
        //UINTN bufferSize = sizeof(bufferChar8);
        //Print(L"4\n");
        //status = logFile->Write(logFile, &bufferSize, &bufferChar8);
        Print(L"FAIL\n");
        return EvaluateKeyResponse();
    }

    for(int i = 0; i < JsonArrayCount(vmList); i++){
        Print(L"%a", JsonValueGetString(JsonObjectGetValue(JsonArrayGetValue(vmList, i), "name")));
        Print(L" - %a\n", JsonValueGetString(JsonObjectGetValue(JsonArrayGetValue(vmList, i), "state")));
    }

    return EvaluateKeyResponse();
}