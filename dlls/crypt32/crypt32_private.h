/*
 * Copyright 2005 Juan Lang
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __CRYPT32_PRIVATE_H__
#define __CRYPT32_PRIVATE_H__

/* a few asn.1 tags we need */
#define ASN_BOOL            (ASN_UNIVERSAL | ASN_PRIMITIVE | 0x01)
#define ASN_BITSTRING       (ASN_UNIVERSAL | ASN_PRIMITIVE | 0x03)
#define ASN_ENUMERATED      (ASN_UNIVERSAL | ASN_PRIMITIVE | 0x0a)
#define ASN_SETOF           (ASN_UNIVERSAL | ASN_PRIMITIVE | 0x11)
#define ASN_NUMERICSTRING   (ASN_UNIVERSAL | ASN_PRIMITIVE | 0x12)
#define ASN_PRINTABLESTRING (ASN_UNIVERSAL | ASN_PRIMITIVE | 0x13)
#define ASN_IA5STRING       (ASN_UNIVERSAL | ASN_PRIMITIVE | 0x16)
#define ASN_UTCTIME         (ASN_UNIVERSAL | ASN_PRIMITIVE | 0x17)
#define ASN_GENERALTIME     (ASN_UNIVERSAL | ASN_PRIMITIVE | 0x18)

/* The following aren't defined in wincrypt.h, as they're "reserved" */
#define CERT_CERT_PROP_ID 32
#define CERT_CRL_PROP_ID  33
#define CERT_CTL_PROP_ID  34

/* Returns a handle to the default crypto provider; loads it if necessary.
 * Returns NULL on failure.
 */
HCRYPTPROV CRYPT_GetDefaultProvider(void);

void crypt_oid_init(HINSTANCE hinst);
void crypt_oid_free(void);

/* Some typedefs that make it easier to abstract which type of context we're
 * working with.
 */
typedef const void *(WINAPI *CreateContextFunc)(DWORD dwCertEncodingType,
 const BYTE *pbCertEncoded, DWORD cbCertEncoded);
typedef BOOL (WINAPI *AddContextToStoreFunc)(HCERTSTORE hCertStore,
 const void *context, DWORD dwAddDisposition, const void **ppStoreContext);
typedef BOOL (WINAPI *AddEncodedContextToStoreFunc)(HCERTSTORE hCertStore,
 DWORD dwCertEncodingType, const BYTE *pbEncoded, DWORD cbEncoded,
 DWORD dwAddDisposition, const void **ppContext);
typedef const void *(WINAPI *DuplicateContextFunc)(const void *context);
typedef const void *(WINAPI *EnumContextsInStoreFunc)(HCERTSTORE hCertStore,
 const void *pPrevContext);
typedef BOOL (WINAPI *GetContextPropertyFunc)(const void *context,
 DWORD dwPropID, void *pvData, DWORD *pcbData);
typedef BOOL (WINAPI *SetContextPropertyFunc)(const void *context,
 DWORD dwPropID, DWORD dwFlags, const void *pvData);
typedef BOOL (WINAPI *SerializeElementFunc)(const void *context, DWORD dwFlags,
 BYTE *pbElement, DWORD *pcbElement);
typedef BOOL (WINAPI *FreeContextFunc)(const void *context);
typedef BOOL (WINAPI *DeleteContextFunc)(const void *context);

/* An abstract context (certificate, CRL, or CTL) interface */
typedef struct _WINE_CONTEXT_INTERFACE
{
    CreateContextFunc            create;
    AddContextToStoreFunc        addContextToStore;
    AddEncodedContextToStoreFunc addEncodedToStore;
    DuplicateContextFunc         duplicate;
    EnumContextsInStoreFunc      enumContextsInStore;
    GetContextPropertyFunc       getProp;
    SetContextPropertyFunc       setProp;
    SerializeElementFunc         serialize;
    FreeContextFunc              free;
    DeleteContextFunc            deleteFromStore;
} WINE_CONTEXT_INTERFACE, *PWINE_CONTEXT_INTERFACE;
typedef const WINE_CONTEXT_INTERFACE *PCWINE_CONTEXT_INTERFACE;

extern PCWINE_CONTEXT_INTERFACE pCertInterface;
extern PCWINE_CONTEXT_INTERFACE pCRLInterface;
extern PCWINE_CONTEXT_INTERFACE pCTLInterface;

/* Helper function for store reading functions and
 * CertAddSerializedElementToStore.  Returns a context of the appropriate type
 * if it can, or NULL otherwise.  Doesn't validate any of the properties in
 * the serialized context (for example, bad hashes are retained.)
 * *pdwContentType is set to the type of the returned context.
 */
const void *CRYPT_ReadSerializedElement(const BYTE *pbElement,
 DWORD cbElement, DWORD dwContextTypeFlags, DWORD *pdwContentType);

/**
 *  Context property list functions
 */
struct _CONTEXT_PROPERTY_LIST;
typedef struct _CONTEXT_PROPERTY_LIST *PCONTEXT_PROPERTY_LIST;

PCONTEXT_PROPERTY_LIST ContextPropertyList_Create(void);

/* Searches for the property with ID id in the context.  Returns TRUE if found,
 * and copies the property's length and a pointer to its data to blob.
 * Otherwise returns FALSE.
 */
BOOL ContextPropertyList_FindProperty(PCONTEXT_PROPERTY_LIST list, DWORD id,
 PCRYPT_DATA_BLOB blob);

BOOL ContextPropertyList_SetProperty(PCONTEXT_PROPERTY_LIST list, DWORD id,
 const BYTE *pbData, size_t cbData);

void ContextPropertyList_RemoveProperty(PCONTEXT_PROPERTY_LIST list, DWORD id);

DWORD ContextPropertyList_EnumPropIDs(PCONTEXT_PROPERTY_LIST list, DWORD id);

void ContextPropertyList_Copy(PCONTEXT_PROPERTY_LIST to,
 PCONTEXT_PROPERTY_LIST from);

void ContextPropertyList_Free(PCONTEXT_PROPERTY_LIST list);

#endif
