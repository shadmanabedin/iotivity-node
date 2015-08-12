#include <node_buffer.h>
#include <nan.h>

extern "C" {
#include <string.h>
}

#include "structures.h"
#include "common.h"

using namespace v8;
using namespace node;

bool c_OCEntityHandlerResponse(OCEntityHandlerResponse *destination,
                               v8::Local<Object> jsOCEntityHandlerResponse) {
  // requestHandle
  Local<Value> requestHandle =
      jsOCEntityHandlerResponse->Get(NanNew<String>("requestHandle"));
  if (!Buffer::HasInstance(requestHandle)) {
    NanThrowTypeError("requestHandle is not a Node::Buffer");
    return false;
  }
  destination->requestHandle =
      *(OCRequestHandle *)Buffer::Data(requestHandle->ToObject());

  // responseHandle is filled in by the stack

  // resourceHandle
  Local<Value> resourceHandle =
      jsOCEntityHandlerResponse->Get(NanNew<String>("resourceHandle"));
  if (!Buffer::HasInstance(resourceHandle)) {
    NanThrowTypeError("resourceHandle is not a Node::Buffer");
    return false;
  }
  destination->resourceHandle =
      *(OCResourceHandle *)Buffer::Data(resourceHandle->ToObject());

  // ehResult
  Local<Value> ehResult =
      jsOCEntityHandlerResponse->Get(NanNew<String>("ehResult"));
  VALIDATE_VALUE_TYPE(ehResult, IsUint32, "ehResult", false);
  destination->ehResult = (OCEntityHandlerResult)ehResult->Uint32Value();

  // payload and payloadSize
  Local<Value> payload =
      jsOCEntityHandlerResponse->Get(NanNew<String>("payload"));
  VALIDATE_VALUE_TYPE(payload, IsString, "payload", false);

  // Make sure the size in bytes of the UTF-8 representation, including the
  // terminating NULL
  // character, does not exceed MAX_RESPONSE_LENGTH
  size_t payloadLength = (size_t)payload->ToString()->Utf8Length();
  if (payloadLength >= MAX_RESPONSE_LENGTH) {
    NanThrowRangeError("payload is longer than MAX_RESPONSE_LENGTH");
    return false;
  }

  // numSendVendorSpecificHeaderOptions
  Local<Value> numSendVendorSpecificHeaderOptions =
      jsOCEntityHandlerResponse->Get(
          NanNew<String>("numSendVendorSpecificHeaderOptions"));
  VALIDATE_VALUE_TYPE(numSendVendorSpecificHeaderOptions, IsUint32,
                      "numSendVendorSpecificHeaderOptions", false);
  uint8_t headerOptionCount =
      (uint8_t)numSendVendorSpecificHeaderOptions->Uint32Value();
  if (headerOptionCount > MAX_HEADER_OPTIONS) {
    NanThrowRangeError(
        "numSendVendorSpecificHeaderOptions is larger than MAX_HEADER_OPTIONS");
    return false;
  }
  destination->numSendVendorSpecificHeaderOptions = headerOptionCount;

  // sendVendorSpecificHeaderOptions
  int headerOptionIndex, optionDataIndex;
  Local<Value> headerOptionsValue = jsOCEntityHandlerResponse->Get(
      NanNew<String>("sendVendorSpecificHeaderOptions"));
  VALIDATE_VALUE_TYPE(headerOptionsValue, IsArray,
                      "sendVendorSpecificHeaderOptions", false);
  Local<Array> headerOptions = Local<Array>::Cast(headerOptionsValue);
  for (headerOptionIndex = 0; headerOptionIndex < headerOptionCount;
       headerOptionIndex++) {
    Local<Value> headerOptionValue = headerOptions->Get(headerOptionIndex);
    VALIDATE_VALUE_TYPE(headerOptionValue, IsObject,
                        "sendVendorSpecificHeaderOptions member", false);
    Local<Object> headerOption = headerOptionValue->ToObject();

    // sendVendorSpecificHeaderOptions[].protocolID
    Local<Value> protocolIDValue =
        headerOption->Get(NanNew<String>("protocolID"));
    VALIDATE_VALUE_TYPE(protocolIDValue, IsUint32, "protocolID", false);
    destination->sendVendorSpecificHeaderOptions[headerOptionIndex].protocolID =
        (OCTransportProtocolID)protocolIDValue->Uint32Value();

    // sendVendorSpecificHeaderOptions[].optionID
    Local<Value> optionIDValue = headerOption->Get(NanNew<String>("optionID"));
    VALIDATE_VALUE_TYPE(optionIDValue, IsUint32, "optionID", false);
    destination->sendVendorSpecificHeaderOptions[headerOptionIndex].optionID =
        (uint16_t)protocolIDValue->Uint32Value();

    // sendVendorSpecificHeaderOptions[].optionLength
    Local<Value> optionLengthValue =
        headerOption->Get(NanNew<String>("optionLength"));
    VALIDATE_VALUE_TYPE(optionLengthValue, IsUint32, "optionLength", false);
    uint16_t optionLength = (uint16_t)optionLengthValue->Uint32Value();
    if (optionLength > MAX_HEADER_OPTION_DATA_LENGTH) {
      NanThrowRangeError(
          "optionLength is larger than MAX_HEADER_OPTION_DATA_LENGTH");
      return false;
    }
    destination->sendVendorSpecificHeaderOptions[headerOptionIndex]
        .optionLength = optionLength;

    // sendVendorSpecificHeaderOptions[].optionData
    Local<Value> optionDataValue =
        headerOption->Get(NanNew<String>("optionData"));
    VALIDATE_VALUE_TYPE(optionDataValue, IsArray, "optionData", false);
    Local<Array> optionData = Local<Array>::Cast(optionDataValue);
    for (optionDataIndex = 0; optionDataIndex < optionLength;
         optionDataIndex++) {
      Local<Value> optionDataItemValue = optionData->Get(optionDataIndex);
      VALIDATE_VALUE_TYPE(optionDataItemValue, IsUint32, "optionData item",
                          false);
      destination->sendVendorSpecificHeaderOptions[headerOptionIndex]
          .optionData[optionDataIndex] =
          (uint8_t)optionDataItemValue->Uint32Value();
    }
  }

  return true;
}

Local<Object> js_OCResourceHandle(Local<Object> jsHandle,
                                  OCResourceHandle handle) {
  jsHandle->Set(
      NanNew<String>("handle"),
      NanNewBufferHandle((const char *)&handle, sizeof(OCResourceHandle)));

  return jsHandle;
}

bool c_OCResourceHandle(OCResourceHandle *destination, Local<Object> jsHandle) {
  Local<Value> handle = jsHandle->Get(NanNew<String>("handle"));

  if (!Buffer::HasInstance(handle)) {
    NanThrowTypeError("OCResourceHandle.handle is not a Node::Buffer");
    return false;
  }

  *destination = *(OCResourceHandle *)Buffer::Data(handle->ToObject());

  return true;
}

// Returns the Local<Object> which was passed in
static Local<Object> js_OCRequestHandle(Local<Object> jsHandle,
                                        OCRequestHandle handle) {
  jsHandle->Set(
      NanNew<String>("handle"),
      NanNewBufferHandle((const char *)&handle, sizeof(OCRequestHandle)));

  return jsHandle;
}

Local<Object> js_OCEntityHandlerRequest(OCEntityHandlerRequest *request) {
  Local<Object> jsRequest = NanNew<Object>();

  jsRequest->Set(NanNew<String>("resource"),
                 js_OCResourceHandle(NanNew<Object>(), request->resource));

  jsRequest->Set(NanNew<String>("requestHandle"),
                 js_OCRequestHandle(NanNew<Object>(), request->requestHandle));

  jsRequest->Set(NanNew<String>("method"), NanNew<Number>(request->method));
  if (request->query) {
    jsRequest->Set(NanNew<String>("query"), NanNew<String>(request->query));
  }

  Local<Object> obsInfo = NanNew<Object>();
  obsInfo->Set(NanNew<String>("action"),
               NanNew<Number>(request->obsInfo.action));
  obsInfo->Set(NanNew<String>("obsId"), NanNew<Number>(request->obsInfo.obsId));
  jsRequest->Set(NanNew<String>("obsInfo"), obsInfo);

	jsRequest->Set( NanNew<String>( "rcvdVendorSpecificHeaderOptions" ),
		js_OCHeaderOption( request->rcvdVendorSpecificHeaderOptions, request->numRcvdVendorSpecificHeaderOptions ) );

	jsRequest->Set( NanNew<String>( "devAddr" ), js_OCDevAddr( &( request->devAddr ) ) );
	if ( request->payload ) {
		jsRequest->Set( NanNew<String>( "payload" ), js_OCPayload( request->payload ) );
	}

  return jsRequest;
}
