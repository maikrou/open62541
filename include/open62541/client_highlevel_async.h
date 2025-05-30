/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 *    Copyright 2018 (c) Thomas Stalder, Blue Time Concept SA
 *    Copyright 2018, 2025 (c) Fraunhofer IOSB (Author: Julius Pfrommer)
 */

#ifndef UA_CLIENT_HIGHLEVEL_ASYNC_H_
#define UA_CLIENT_HIGHLEVEL_ASYNC_H_

#include <open62541/client.h>
#include <open62541/common.h>
#include <open62541/types.h>

_UA_BEGIN_DECLS

/**
 * .. _client-async-services:
 *
 * Asynchronous Services
 * ---------------------
 *
 * All OPC UA services are asynchronous in nature. So several service calls can
 * be made without waiting for the individual responses. Depending on the
 * server's priorities responses may come in a different ordering than sent.
 *
 * Connection and session management are performed in `UA_Client_run_iterate`,
 * so to keep a connection healthy any client needs to consider how and when it
 * is appropriate to do the call. This is especially true for the periodic
 * renewal of a SecureChannel's SecurityToken which is designed to have a
 * limited lifetime and will invalidate the connection if not renewed.
 *
 * If there is an error after an async service has been dispatched, the callback
 * is called with an "empty" response where the StatusCode has been set
 * accordingly. This is also done if the client is shutting down and the list of
 * dispatched async services is emptied. The StatusCode received when the client
 * is shutting down is UA_STATUSCODE_BADSHUTDOWN. The StatusCode received when
 * the client doesn't receive response after the specified in config->timeout
 * (can be overridden via the "timeoutHint" in the request header) is
 * UA_STATUSCODE_BADTIMEOUT.
 *
 * The userdata and requestId arguments can be NULL. The (optional) requestId
 * output can be used to cancel the service while it is still pending. The
 * requestId is unique for each service request. Alternatively the requestHandle
 * can be manually set (non necessarily unique) in the request header for full
 * service call. This can be used to cancel all outstanding requests using that
 * handle together. Note that the client will auto-generate a requestHandle
 * >100,000 if none is defined. Avoid these when manually setting a requetHandle
 * in the requestHeader to avoid clashes. */

/* Generalized asynchronous service call. This can be used for any
 * request/response datatype pair whenever no type-stable specialization is
 * defined below. */
typedef void
(*UA_ClientAsyncServiceCallback)(UA_Client *client, void *userdata,
                                 UA_UInt32 requestId, void *response);

UA_StatusCode UA_EXPORT UA_THREADSAFE
__UA_Client_AsyncService(UA_Client *client, const void *request,
                         const UA_DataType *requestType,
                         UA_ClientAsyncServiceCallback callback,
                         const UA_DataType *responseType,
                         void *userdata, UA_UInt32 *requestId);

/* Cancel all dispatched requests with the given requestHandle.
 * The number if cancelled requests is returned by the server.
 * The output argument cancelCount is not set if NULL. */
UA_EXPORT UA_THREADSAFE UA_StatusCode
UA_Client_cancelByRequestHandle(UA_Client *client, UA_UInt32 requestHandle,
                                UA_UInt32 *cancelCount);

/* Map the requestId to the requestHandle used for that request and call the
 * Cancel service for that requestHandle. */
UA_EXPORT UA_THREADSAFE UA_StatusCode
UA_Client_cancelByRequestId(UA_Client *client, UA_UInt32 requestId,
                            UA_UInt32 *cancelCount);

/* Force the manual renewal of the SecureChannel. This is useful to renew the
 * SecureChannel during a downtime when no time-critical operations are
 * performed. This method is asynchronous. The renewal is triggered (the OPN
 * message is sent) but not completed. The OPN response is handled with
 * ``UA_Client_run_iterate`` or a synchronous service-call operation.
 *
 * @return The return value is UA_STATUSCODE_GOODCALLAGAIN if the SecureChannel
 *         has not elapsed at least 75% of its lifetime. Otherwise the
 *         ``connectStatus`` is returned. */
UA_StatusCode UA_EXPORT UA_THREADSAFE
UA_Client_renewSecureChannel(UA_Client *client);

/**
 * Asynchronous Service Calls
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * Call OPC UA Services asynchronously with a callback. The (optional) requestId
 * output can be used to cancel the service while it is still pending. */

typedef void
(*UA_ClientAsyncReadCallback)(UA_Client *client, void *userdata,
                              UA_UInt32 requestId, UA_ReadResponse *rr);

UA_StatusCode UA_EXPORT UA_THREADSAFE
UA_Client_sendAsyncReadRequest(UA_Client *client, UA_ReadRequest *request,
                               UA_ClientAsyncReadCallback readCallback,
                               void *userdata, UA_UInt32 *reqId);

typedef void
(*UA_ClientAsyncWriteCallback)(UA_Client *client, void *userdata,
                               UA_UInt32 requestId, UA_WriteResponse *wr);

UA_StatusCode UA_EXPORT UA_THREADSAFE
UA_Client_sendAsyncWriteRequest(UA_Client *client, UA_WriteRequest *request,
                                UA_ClientAsyncWriteCallback writeCallback,
                                void *userdata, UA_UInt32 *reqId);

typedef void
(*UA_ClientAsyncBrowseCallback)(UA_Client *client, void *userdata,
                                UA_UInt32 requestId, UA_BrowseResponse *wr);

UA_StatusCode UA_EXPORT UA_THREADSAFE
UA_Client_sendAsyncBrowseRequest(UA_Client *client, UA_BrowseRequest *request,
                                 UA_ClientAsyncBrowseCallback browseCallback,
                                 void *userdata, UA_UInt32 *reqId);

typedef void
(*UA_ClientAsyncBrowseNextCallback)(UA_Client *client, void *userdata,
                                    UA_UInt32 requestId,
                                    UA_BrowseNextResponse *wr);

UA_StatusCode UA_EXPORT UA_THREADSAFE
UA_Client_sendAsyncBrowseNextRequest(
    UA_Client *client, UA_BrowseNextRequest *request,
    UA_ClientAsyncBrowseNextCallback browseNextCallback,
    void *userdata, UA_UInt32 *reqId);

/**
 * Asynchronous Operations
 * ~~~~~~~~~~~~~~~~~~~~~~~
 *
 * Many Services can be called with an array of operations. For example, a
 * request to the Read Service contains an array of ReadValueId, each
 * corresponding to a single read operation. For convenience, wrappers are
 * provided to call single operations for the most common Services.
 *
 * All async operations have a callback of the following structure: The returned
 * StatusCode is split in two parts. The status indicates the overall success of
 * the request and the operation. The result argument is non-NULL only if the
 * status is no good. */

typedef void
(*UA_ClientAsyncOperationCallback)(
    UA_Client *client, void *userdata, UA_UInt32 requestId,
    UA_StatusCode status, void *result);

/**
 * Read Attribute
 * ^^^^^^^^^^^^^^
 *
 * Asynchronously read a single attribute. The attribute is unpacked from the
 * response as the datatype of the attribute is known ahead of time. Value
 * attributes are variants.
 *
 * Note that the last argument (value pointer) of the callbacks can be NULL if
 * the status of the operation is not good. */

/* Reading a single attribute */
typedef void
(*UA_ClientAsyncReadAttributeCallback)(
    UA_Client *client, void *userdata, UA_UInt32 requestId,
    UA_StatusCode status, UA_DataValue *attribute);

UA_StatusCode UA_EXPORT UA_THREADSAFE
UA_Client_readAttribute_async(
    UA_Client *client, const UA_ReadValueId *rvi,
    UA_TimestampsToReturn timestampsToReturn,
    UA_ClientAsyncReadAttributeCallback callback,
    void *userdata, UA_UInt32 *requestId);

/* Read a single Value attribute */
typedef void
(*UA_ClientAsyncReadValueAttributeCallback)(
    UA_Client *client, void *userdata, UA_UInt32 requestId,
    UA_StatusCode status, UA_DataValue *value);

UA_StatusCode UA_EXPORT UA_THREADSAFE
UA_Client_readValueAttribute_async(
    UA_Client *client, const UA_NodeId nodeId,
    UA_ClientAsyncReadValueAttributeCallback callback,
    void *userdata, UA_UInt32 *requestId);

/* Read a single DataType attribute */
typedef void
(*UA_ClientAsyncReadDataTypeAttributeCallback)(
    UA_Client *client, void *userdata, UA_UInt32 requestId,
    UA_StatusCode status, UA_NodeId *dataType);

UA_StatusCode UA_EXPORT UA_THREADSAFE
UA_Client_readDataTypeAttribute_async(
    UA_Client *client, const UA_NodeId nodeId,
    UA_ClientAsyncReadDataTypeAttributeCallback callback,
    void *userdata, UA_UInt32 *requestId);

/* Read a single ArrayDimensions attribute. If the status is good, the variant
 * carries an UInt32 array. */
typedef void
(*UA_ClientReadArrayDimensionsAttributeCallback)(
    UA_Client *client, void *userdata, UA_UInt32 requestId,
    UA_StatusCode status, UA_Variant *arrayDimensions);

UA_StatusCode UA_EXPORT UA_THREADSAFE
UA_Client_readArrayDimensionsAttribute_async(
    UA_Client *client, const UA_NodeId nodeId,
    UA_ClientReadArrayDimensionsAttributeCallback callback,
    void *userdata, UA_UInt32 *requestId);

/* Read a single NodeClass attribute */
typedef void
(*UA_ClientAsyncReadNodeClassAttributeCallback)(
    UA_Client *client, void *userdata, UA_UInt32 requestId,
    UA_StatusCode status, UA_NodeClass *nodeClass);

UA_StatusCode UA_EXPORT UA_THREADSAFE
UA_Client_readNodeClassAttribute_async(
    UA_Client *client, const UA_NodeId nodeId,
    UA_ClientAsyncReadNodeClassAttributeCallback callback,
    void *userdata, UA_UInt32 *requestId);

/* Read a single BrowseName attribute */
typedef void
(*UA_ClientAsyncReadBrowseNameAttributeCallback)(
    UA_Client *client, void *userdata, UA_UInt32 requestId,
    UA_StatusCode status, UA_QualifiedName *browseName);

UA_StatusCode UA_EXPORT UA_THREADSAFE
UA_Client_readBrowseNameAttribute_async(
    UA_Client *client, const UA_NodeId nodeId,
    UA_ClientAsyncReadBrowseNameAttributeCallback callback,
    void *userdata, UA_UInt32 *requestId);

/* Read a single DisplayName attribute */
typedef void
(*UA_ClientAsyncReadDisplayNameAttributeCallback)(
    UA_Client *client, void *userdata, UA_UInt32 requestId,
    UA_StatusCode status, UA_LocalizedText *displayName);

UA_StatusCode UA_EXPORT UA_THREADSAFE
UA_Client_readDisplayNameAttribute_async(
    UA_Client *client, const UA_NodeId nodeId,
    UA_ClientAsyncReadDisplayNameAttributeCallback callback,
    void *userdata, UA_UInt32 *requestId);

/* Read a single Description attribute */
typedef void
(*UA_ClientAsyncReadDescriptionAttributeCallback)(
    UA_Client *client, void *userdata, UA_UInt32 requestId,
    UA_StatusCode status, UA_LocalizedText *description);

UA_StatusCode UA_EXPORT UA_THREADSAFE
UA_Client_readDescriptionAttribute_async(
    UA_Client *client, const UA_NodeId nodeId,
    UA_ClientAsyncReadDescriptionAttributeCallback callback,
    void *userdata, UA_UInt32 *requestId);

/* Read a single WriteMask attribute */
typedef void
(*UA_ClientAsyncReadWriteMaskAttributeCallback)(
    UA_Client *client, void *userdata, UA_UInt32 requestId,
    UA_StatusCode status, UA_UInt32 *writeMask);

UA_StatusCode UA_EXPORT UA_THREADSAFE
UA_Client_readWriteMaskAttribute_async(
    UA_Client *client, const UA_NodeId nodeId,
    UA_ClientAsyncReadWriteMaskAttributeCallback callback,
    void *userdata, UA_UInt32 *requestId);

/* Read a single UserWriteMask attribute */
typedef void
(*UA_ClientAsyncReadUserWriteMaskAttributeCallback)(
    UA_Client *client, void *userdata, UA_UInt32 requestId,
    UA_StatusCode status, UA_UInt32 *writeMask);

UA_StatusCode UA_EXPORT UA_THREADSAFE
UA_Client_readUserWriteMaskAttribute_async(
    UA_Client *client, const UA_NodeId nodeId,
    UA_ClientAsyncReadUserWriteMaskAttributeCallback callback,
    void *userdata, UA_UInt32 *requestId);

/* Read a single IsAbstract attribute */
typedef void
(*UA_ClientAsyncReadIsAbstractAttributeCallback)(
    UA_Client *client, void *userdata, UA_UInt32 requestId,
    UA_StatusCode status, UA_Boolean *isAbstract);

UA_StatusCode UA_EXPORT UA_THREADSAFE
UA_Client_readIsAbstractAttribute_async(
    UA_Client *client, const UA_NodeId nodeId,
    UA_ClientAsyncReadIsAbstractAttributeCallback callback,
    void *userdata, UA_UInt32 *requestId);

/* Read a single Symmetric attribute */
typedef void
(*UA_ClientAsyncReadSymmetricAttributeCallback)(
    UA_Client *client, void *userdata, UA_UInt32 requestId,
    UA_StatusCode status, UA_Boolean *symmetric);

UA_StatusCode UA_EXPORT UA_THREADSAFE
UA_Client_readSymmetricAttribute_async(
    UA_Client *client, const UA_NodeId nodeId,
    UA_ClientAsyncReadSymmetricAttributeCallback callback,
    void *userdata, UA_UInt32 *requestId);

/* Read a single InverseName attribute */
typedef void
(*UA_ClientAsyncReadInverseNameAttributeCallback)(
    UA_Client *client, void *userdata, UA_UInt32 requestId,
    UA_StatusCode status, UA_LocalizedText *inverseName);

UA_StatusCode UA_EXPORT UA_THREADSAFE
UA_Client_readInverseNameAttribute_async(
    UA_Client *client, const UA_NodeId nodeId,
    UA_ClientAsyncReadInverseNameAttributeCallback callback,
    void *userdata, UA_UInt32 *requestId);

/* Read a single ContainsNoLoops attribute */
typedef void
(*UA_ClientAsyncReadContainsNoLoopsAttributeCallback)(
    UA_Client *client, void *userdata, UA_UInt32 requestId,
    UA_StatusCode status, UA_Boolean *containsNoLoops);

UA_StatusCode UA_EXPORT UA_THREADSAFE
UA_Client_readContainsNoLoopsAttribute_async(
    UA_Client *client, const UA_NodeId nodeId,
    UA_ClientAsyncReadContainsNoLoopsAttributeCallback callback,
    void *userdata, UA_UInt32 *requestId);

/* Read a single EventNotifier attribute */
typedef void
(*UA_ClientAsyncReadEventNotifierAttributeCallback)(
    UA_Client *client, void *userdata, UA_UInt32 requestId,
    UA_StatusCode status, UA_Byte *eventNotifier);

UA_StatusCode UA_EXPORT UA_THREADSAFE
UA_Client_readEventNotifierAttribute_async(
    UA_Client *client, const UA_NodeId nodeId,
    UA_ClientAsyncReadEventNotifierAttributeCallback callback,
    void *userdata, UA_UInt32 *requestId);

/* Read a single ValueRank attribute */
typedef void
(*UA_ClientAsyncReadValueRankAttributeCallback)(
    UA_Client *client, void *userdata, UA_UInt32 requestId,
    UA_StatusCode status, UA_Int32 *valueRank);

UA_StatusCode UA_EXPORT UA_THREADSAFE
UA_Client_readValueRankAttribute_async(
    UA_Client *client, const UA_NodeId nodeId,
    UA_ClientAsyncReadValueRankAttributeCallback callback,
    void *userdata, UA_UInt32 *requestId);

/* Read a single AccessLevel attribute */
typedef void
(*UA_ClientAsyncReadAccessLevelAttributeCallback)(
    UA_Client *client, void *userdata, UA_UInt32 requestId,
    UA_StatusCode status, UA_Byte *accessLevel);

UA_StatusCode UA_EXPORT UA_THREADSAFE
UA_Client_readAccessLevelAttribute_async(
    UA_Client *client, const UA_NodeId nodeId,
    UA_ClientAsyncReadAccessLevelAttributeCallback callback,
    void *userdata, UA_UInt32 *requestId);

/* Read a single AccessLevelEx attribute */
typedef void
(*UA_ClientAsyncReadAccessLevelExAttributeCallback)(
    UA_Client *client, void *userdata, UA_UInt32 requestId,
    UA_StatusCode status, UA_UInt32 *accessLevelEx);

UA_StatusCode UA_EXPORT UA_THREADSAFE
UA_Client_readAccessLevelExAttribute_async(
    UA_Client *client, const UA_NodeId nodeId,
    UA_ClientAsyncReadAccessLevelExAttributeCallback callback,
    void *userdata, UA_UInt32 *requestId);

/* Read a single UserAccessLevel attribute */
typedef void
(*UA_ClientAsyncReadUserAccessLevelAttributeCallback)(
    UA_Client *client, void *userdata, UA_UInt32 requestId,
    UA_StatusCode status, UA_Byte *userAccessLevel);

UA_StatusCode UA_EXPORT UA_THREADSAFE
UA_Client_readUserAccessLevelAttribute_async(
    UA_Client *client, const UA_NodeId nodeId,
    UA_ClientAsyncReadUserAccessLevelAttributeCallback callback,
    void *userdata, UA_UInt32 *requestId);

/* Read a single MinimumSamplingInterval attribute */
typedef void
(*UA_ClientAsyncReadMinimumSamplingIntervalAttributeCallback)(
    UA_Client *client, void *userdata, UA_UInt32 requestId,
    UA_StatusCode status, UA_Double *minimumSamplingInterval);

UA_StatusCode UA_EXPORT UA_THREADSAFE
UA_Client_readMinimumSamplingIntervalAttribute_async(
    UA_Client *client, const UA_NodeId nodeId,
    UA_ClientAsyncReadMinimumSamplingIntervalAttributeCallback callback,
    void *userdata, UA_UInt32 *requestId);

/* Read a single Historizing attribute */
typedef void
(*UA_ClientAsyncReadHistorizingAttributeCallback)(
    UA_Client *client, void *userdata, UA_UInt32 requestId,
    UA_StatusCode status, UA_Boolean *historizing);

UA_StatusCode UA_EXPORT UA_THREADSAFE
UA_Client_readHistorizingAttribute_async(
    UA_Client *client, const UA_NodeId nodeId,
    UA_ClientAsyncReadHistorizingAttributeCallback callback,
    void *userdata, UA_UInt32 *requestId);

/* Read a single Executable attribute */
typedef void
(*UA_ClientAsyncReadExecutableAttributeCallback)(
    UA_Client *client, void *userdata, UA_UInt32 requestId,
    UA_StatusCode status, UA_Boolean *executable);

UA_StatusCode UA_EXPORT UA_THREADSAFE
UA_Client_readExecutableAttribute_async(
    UA_Client *client, const UA_NodeId nodeId,
    UA_ClientAsyncReadExecutableAttributeCallback callback,
    void *userdata, UA_UInt32 *requestId);

/* Read a single UserExecutable attribute */
typedef void
(*UA_ClientAsyncReadUserExecutableAttributeCallback)(
    UA_Client *client, void *userdata, UA_UInt32 requestId,
    UA_StatusCode status, UA_Boolean *userExecutable);

UA_StatusCode UA_EXPORT UA_THREADSAFE
UA_Client_readUserExecutableAttribute_async(
    UA_Client *client, const UA_NodeId nodeId,
    UA_ClientAsyncReadUserExecutableAttributeCallback callback,
    void *userdata, UA_UInt32 *requestId);

/**
 * Write Attribute
 * ^^^^^^^^^^^^^^^
 * The methods for async writing of attributes all have a similar API. We
 * generate the method signatures with a macro. */

#define UA_CLIENT_ASYNCWRITE(NAME, ATTR_TYPE)                           \
    UA_StatusCode UA_EXPORT UA_THREADSAFE                               \
    NAME(UA_Client *client, const UA_NodeId nodeId,                     \
         const ATTR_TYPE *attr, UA_ClientAsyncWriteCallback callback,   \
         void *userdata, UA_UInt32 *reqId);

UA_CLIENT_ASYNCWRITE(UA_Client_writeNodeIdAttribute_async, UA_NodeId)
UA_CLIENT_ASYNCWRITE(UA_Client_writeNodeClassAttribute_async, UA_NodeClass)
UA_CLIENT_ASYNCWRITE(UA_Client_writeBrowseNameAttribute_async, UA_QualifiedName)
UA_CLIENT_ASYNCWRITE(UA_Client_writeDisplayNameAttribute_async, UA_LocalizedText)
UA_CLIENT_ASYNCWRITE(UA_Client_writeDescriptionAttribute_async, UA_LocalizedText)
UA_CLIENT_ASYNCWRITE(UA_Client_writeWriteMaskAttribute_async, UA_UInt32)
UA_CLIENT_ASYNCWRITE(UA_Client_writeIsAbstractAttribute_async, UA_Boolean)
UA_CLIENT_ASYNCWRITE(UA_Client_writeSymmetricAttribute_async, UA_Boolean)
UA_CLIENT_ASYNCWRITE(UA_Client_writeInverseNameAttribute_async, UA_LocalizedText)
UA_CLIENT_ASYNCWRITE(UA_Client_writeContainsNoLoopsAttribute_async, UA_Boolean)
UA_CLIENT_ASYNCWRITE(UA_Client_writeEventNotifierAttribute_async, UA_Byte)
UA_CLIENT_ASYNCWRITE(UA_Client_writeValueAttribute_async, UA_Variant)
UA_CLIENT_ASYNCWRITE(UA_Client_writeDataTypeAttribute_async, UA_NodeId)
UA_CLIENT_ASYNCWRITE(UA_Client_writeValueRankAttribute_async, UA_Int32)
UA_CLIENT_ASYNCWRITE(UA_Client_writeAccessLevelAttribute_async, UA_Byte)
UA_CLIENT_ASYNCWRITE(UA_Client_writeMinimumSamplingIntervalAttribute_async, UA_Double)
UA_CLIENT_ASYNCWRITE(UA_Client_writeHistorizingAttribute_async, UA_Boolean)
UA_CLIENT_ASYNCWRITE(UA_Client_writeExecutableAttribute_async, UA_Boolean)
UA_CLIENT_ASYNCWRITE(UA_Client_writeAccessLevelExAttribute_async, UA_UInt32)

/**
 * Method Calling
 * ^^^^^^^^^^^^^^ */

typedef void
(*UA_ClientAsyncCallCallback)(
    UA_Client *client, void *userdata,
    UA_UInt32 requestId, UA_CallResponse *cr);

UA_StatusCode UA_EXPORT UA_THREADSAFE
UA_Client_call_async(UA_Client *client, const UA_NodeId objectId,
                     const UA_NodeId methodId, size_t inputSize,
                     const UA_Variant *input,
                     UA_ClientAsyncCallCallback callback,
                     void *userdata, UA_UInt32 *reqId);

/**
 * Node Management
 * ^^^^^^^^^^^^^^^ */

typedef void
(*UA_ClientAsyncAddNodesCallback)(
    UA_Client *client, void *userdata,
    UA_UInt32 requestId, UA_AddNodesResponse *ar);

UA_StatusCode UA_EXPORT UA_THREADSAFE
UA_Client_addVariableNode_async(
    UA_Client *client, const UA_NodeId requestedNewNodeId,
    const UA_NodeId parentNodeId, const UA_NodeId referenceTypeId,
    const UA_QualifiedName browseName, const UA_NodeId typeDefinition,
    const UA_VariableAttributes attr, UA_NodeId *outNewNodeId,
    UA_ClientAsyncAddNodesCallback callback, void *userdata,
    UA_UInt32 *reqId);

UA_StatusCode UA_EXPORT UA_THREADSAFE
UA_Client_addVariableTypeNode_async(
    UA_Client *client, const UA_NodeId requestedNewNodeId,
    const UA_NodeId parentNodeId, const UA_NodeId referenceTypeId,
    const UA_QualifiedName browseName, const UA_VariableTypeAttributes attr,
    UA_NodeId *outNewNodeId, UA_ClientAsyncAddNodesCallback callback,
    void *userdata, UA_UInt32 *reqId);

UA_StatusCode UA_EXPORT UA_THREADSAFE
UA_Client_addObjectNode_async(
    UA_Client *client, const UA_NodeId requestedNewNodeId,
    const UA_NodeId parentNodeId, const UA_NodeId referenceTypeId,
    const UA_QualifiedName browseName, const UA_NodeId typeDefinition,
    const UA_ObjectAttributes attr, UA_NodeId *outNewNodeId,
    UA_ClientAsyncAddNodesCallback callback, void *userdata,
    UA_UInt32 *reqId);

UA_StatusCode UA_EXPORT UA_THREADSAFE
UA_Client_addObjectTypeNode_async(
    UA_Client *client, const UA_NodeId requestedNewNodeId,
    const UA_NodeId parentNodeId, const UA_NodeId referenceTypeId,
    const UA_QualifiedName browseName, const UA_ObjectTypeAttributes attr,
    UA_NodeId *outNewNodeId, UA_ClientAsyncAddNodesCallback callback,
    void *userdata, UA_UInt32 *reqId);

UA_StatusCode UA_EXPORT UA_THREADSAFE
UA_Client_addViewNode_async(
    UA_Client *client, const UA_NodeId requestedNewNodeId,
    const UA_NodeId parentNodeId, const UA_NodeId referenceTypeId,
    const UA_QualifiedName browseName,
    const UA_ViewAttributes attr, UA_NodeId *outNewNodeId,
    UA_ClientAsyncAddNodesCallback callback, void *userdata,
    UA_UInt32 *reqId);

UA_StatusCode UA_EXPORT UA_THREADSAFE
UA_Client_addReferenceTypeNode_async(
    UA_Client *client, const UA_NodeId requestedNewNodeId,
    const UA_NodeId parentNodeId, const UA_NodeId referenceTypeId,
    const UA_QualifiedName browseName, const UA_ReferenceTypeAttributes attr,
    UA_NodeId *outNewNodeId, UA_ClientAsyncAddNodesCallback callback,
    void *userdata, UA_UInt32 *reqId);

UA_StatusCode UA_EXPORT UA_THREADSAFE
UA_Client_addDataTypeNode_async(
    UA_Client *client, const UA_NodeId requestedNewNodeId,
    const UA_NodeId parentNodeId, const UA_NodeId referenceTypeId,
    const UA_QualifiedName browseName, const UA_DataTypeAttributes attr,
    UA_NodeId *outNewNodeId, UA_ClientAsyncAddNodesCallback callback,
    void *userdata, UA_UInt32 *reqId);

UA_StatusCode UA_EXPORT UA_THREADSAFE
UA_Client_addMethodNode_async(
    UA_Client *client, const UA_NodeId requestedNewNodeId,
    const UA_NodeId parentNodeId, const UA_NodeId referenceTypeId,
    const UA_QualifiedName browseName, const UA_MethodAttributes attr,
    UA_NodeId *outNewNodeId, UA_ClientAsyncAddNodesCallback callback,
    void *userdata, UA_UInt32 *reqId);

_UA_END_DECLS

#endif /* UA_CLIENT_HIGHLEVEL_ASYNC_H_ */
