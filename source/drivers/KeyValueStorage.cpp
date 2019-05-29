/*
The MIT License (MIT)

Copyright (c) 2016 British Broadcasting Corporation.
This software is provided by Lancaster University by arrangement with the BBC.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

/**
  * Class definition for the KeyValueStorage class.
  * This allows reading and writing of FLASH memory.
  */

#include "CodalConfig.h"
#include "KeyValueStorage.h"
#include "CodalCompat.h"
#include "CodalDmesg.h"

#ifndef DEVICE_KEY_VALUE_STORE_OFFSET
#define DEVICE_KEY_VALUE_STORE_OFFSET       4
#endif

using namespace codal;


/**
  * Default constructor.
  *
  * Creates an instance of KeyValueStorage which acts like a KeyValueStore
  * that allows the retrieval, addition and deletion of KeyValuePairs.
  */
KeyValueStorage::KeyValueStorage(NVMController& controller) : controller(controller)
{
    memset(scratch, 0, KEY_VALUE_STORAGE_SCRATCH_WORD_SIZE * 4);
    this->flashPagePtr = controller.getFlashEnd() - (controller.getPageSize() * DEVICE_KEY_VALUE_STORE_OFFSET);
    size();
}

/**
  * Method for erasing a page in flash.
  *
  * @param page_address Address of the first word in the page to be erased.
  */
void KeyValueStorage::flashPageErase(uint32_t * page_address)
{
    controller.erase(page_address);
}

/**
  * Function for copying words from one location to another.
  *
  * @param from the address to copy data from.
  *
  * @param to the address to copy the data to.
  *
  * @param sizeInWords the number of words to copy
  */
void KeyValueStorage::flashCopy(uint32_t* from, uint32_t* to, int sizeInWords)
{
    controller.write(to, from, sizeInWords);
}

/**
  * Method for writing a word of data in flash with a value.
  *
  * @param address Address of the word to change.
  *
  * @param value Value to be written to flash.
  */
void KeyValueStorage::flashWordWrite(uint32_t * address, uint32_t value)
{
    controller.write(&value, address, 1);
}

/**
  * Function for populating the scratch page with a KeyValueStore.
  *
  * @param store the KeyValueStore struct to write to the scratch page.
  */
void KeyValueStorage::scratchKeyValueStore(KeyValueStore store)
{
    memcpy(this->scratch, &store, sizeof(KeyValueStore));
}

/**
  * Function for populating the scratch page with a KeyValuePair.
  *
  * @param pair the KeyValuePair struct to write to the scratch page.
  *
  * @param flashPointer the pointer in flash where this KeyValuePair resides. This pointer
  * is used to determine the offset into the scratch page, where the KeyValuePair should
  * be written.
  */
void KeyValueStorage::scratchKeyValuePair(KeyValuePair pair, int scratchOffset)
{
    memcpy(this->scratch + scratchOffset, &pair, sizeof(KeyValuePair));
}

/**
  * Places a given key, and it's corresponding value into flash at the earliest
  * available point.
  *
  * @param key the unique name that should be used as an identifier for the given data.
  *            The key is presumed to be null terminated.
  *
  * @param data a pointer to the beginning of the data to be persisted.
  *
  * @param dataSize the size of the data to be persisted
  *
  * @return DEVICE_OK on success, KEY_VALUE_INVALID_PARAMETER if the key or size is too large,
  *         KEY_VALUE_NO_RESOURCES if the storage page is full
  */
int KeyValueStorage::put(const char *key, uint8_t *data, int dataSize)
{
    KeyValuePair pair;
    int keySize = strlen(key) + 1;

    if(keySize > (int)sizeof(pair.key) || dataSize > (int)sizeof(pair.value) || dataSize < 0)
        return DEVICE_INVALID_PARAMETER;

    KeyValuePair *currentValue = get(key);

    int upToDate = currentValue && (memcmp(currentValue->value, data, dataSize) == 0);

    if(currentValue)
        delete currentValue;

    if(upToDate)
        return DEVICE_OK;

    memcpy(pair.key, key, keySize);
    memcpy(pair.value, data, dataSize);

    //calculate our various offsets.
    uint32_t kvStoreSize = sizeof(KeyValueStore) / 4;
    uint32_t kvPairSize = sizeof(KeyValuePair) / 4;

    uint32_t* flashPointer = this->flashPagePtr;

    int storeSize = size();

    //our KeyValueStore struct is always at 0
    flashPointer += kvStoreSize;
    int scratchPtr = kvStoreSize;

    KeyValuePair storedPair = KeyValuePair();

    int found = 0;

    //erase our scratch page
    memset(scratch, 0, sizeof(scratch));

    //iterate through key value pairs in flash, writing them to the scratch page.
    for(int i = 0; i < storeSize; i++)
    {
        memcpy(&storedPair, flashPointer, sizeof(KeyValuePair));

        //check if the keys match...
        if(strcmp((char *)storedPair.key, (char *)pair.key) == 0)
        {
            found = 1;
            //scratch our KeyValueStore struct so that it is preserved.
            scratchKeyValueStore(KeyValueStore(KEY_VALUE_STORAGE_MAGIC, storeSize));
            scratchKeyValuePair(pair, scratchPtr);
        }
        else
        {
            scratchKeyValuePair(storedPair, scratchPtr);
        }

        flashPointer += kvPairSize;
        scratchPtr += kvPairSize;
    }

    if(!found)
    {
        //if we haven't got a match for the key, check we can add a new KeyValuePair
        if(storeSize == KEY_VALUE_STORAGE_MAX_PAIRS)
            return DEVICE_NO_RESOURCES;

        storeSize += 1;

        //scratch our updated values.
        scratchKeyValueStore(KeyValueStore(KEY_VALUE_STORAGE_MAGIC, storeSize));
        scratchKeyValuePair(pair, scratchPtr);
    }

    //erase our storage page
    flashPageErase(this->flashPagePtr);

    //copy from scratch to storage.
    flashCopy(this->scratch, flashPagePtr, KEY_VALUE_STORAGE_SCRATCH_WORD_SIZE);

    return DEVICE_OK;
}

/**
  * Places a given key, and it's corresponding value into flash at the earliest
  * available point.
  *
  * @param key the unique name that should be used as an identifier for the given data.
  *
  * @param data a pointer to the beginning of the data to be persisted.
  *
  * @param dataSize the size of the data to be persisted
  *
  * @return DEVICE_OK on success, KEY_VALUE_INVALID_PARAMETER if the key or size is too large,
  *         KEY_VALUE_NO_RESOURCES if the storage page is full
  */
int KeyValueStorage::put(ManagedString key, uint8_t* data, int dataSize)
{
    return put((char *)key.toCharArray(), data, dataSize);
}

/**
  * Retreives a KeyValuePair identified by a given key.
  *
  * @param key the unique name used to identify a KeyValuePair in flash.
  *
  * @return a pointer to a heap allocated KeyValuePair struct, this pointer will be
  *         NULL if the key was not found in storage.
  *
  * @note it is up to the user to free memory after use.
  */
KeyValuePair* KeyValueStorage::get(const char* key)
{
    //calculate our offsets for our storage page
    int storeSize = size();

    //we haven't got anything stored, so return...
    if(storeSize == 0)
        return NULL;

    uint32_t* flashPtr = this->flashPagePtr;

    //our KeyValueStore struct is always at 0
    flashPtr += sizeof(KeyValueStore) / 4;

    KeyValuePair *pair = new KeyValuePair();

    int i;

    //iterate through flash until we have a match, or drop out.
    for(i = 0; i < storeSize; i++)
    {
        memcpy(pair, flashPtr, sizeof(KeyValuePair));
        // DMESG("k %s value: %d %d %d %d",pair->key, pair->value[0], pair->value[1], pair->value[2], pair->value[3]);
        if(strcmp(key,(char *)pair->key) == 0)
            break;

        flashPtr += sizeof(KeyValuePair) / 4;
    }

    //clean up
    if(i == storeSize)
    {
        delete pair;
        return NULL;
    }

    return pair;
}

/**
  * Retreives a KeyValuePair identified by a given key.
  *
  * @param key the unique name used to identify a KeyValuePair in flash.
  *
  * @return a pointer to a heap allocated KeyValuePair struct, this pointer will be
  *         NULL if the key was not found in storage.
  *
  * @note it is up to the user to free memory after use.
  */
KeyValuePair* KeyValueStorage::get(ManagedString key)
{
    return get((char *)key.toCharArray());
}

/**
  * Removes a KeyValuePair identified by a given key.
  *
  * @param key the unique name used to identify a KeyValuePair in flash.
  *
  * @return DEVICE_OK on success, or DEVICE_NO_DATA if the given key
  *         was not found in flash.
  */
int KeyValueStorage::remove(const char* key)
{
    uint32_t kvStoreSize = sizeof(KeyValueStore) / 4;
    uint32_t kvPairSize = sizeof(KeyValuePair) / 4;

    int storeSize = size();

    //if we have no data, we have nothing to do.
    if(storeSize == 0)
        return DEVICE_NO_DATA;

    uint32_t* flashPointer = this->flashPagePtr;

    //our KeyValueStore struct is always at 0
    flashPointer += kvStoreSize;
    int scratchPointer = kvStoreSize;

    KeyValuePair storedPair = KeyValuePair();

    int found = 0;

    memset(scratch, 0, KEY_VALUE_STORAGE_SCRATCH_WORD_SIZE * 4);

    // scratch the old size (it will be updated later if required).
    scratchKeyValueStore(KeyValueStore(KEY_VALUE_STORAGE_MAGIC, storeSize));

    //iterate through our flash copy pairs to scratch, unless there is a key patch
    for(int i = 0; i < storeSize; i++)
    {
        memcpy(&storedPair, flashPointer, sizeof(KeyValuePair));

        //if we have a match, don't increment our scratchPointer
        if(strcmp((char *)storedPair.key, (char *)key) == 0)
        {
            found = 1;
            //write our new KeyValueStore data
            scratchKeyValueStore(KeyValueStore(KEY_VALUE_STORAGE_MAGIC, storeSize - 1));
        }
        else
        {
            //otherwise copy the KeyValuePair from our storage page.
            scratchKeyValuePair(storedPair, scratchPointer);
            scratchPointer += sizeof(KeyValuePair) / 4;
        }

        flashPointer += sizeof(KeyValuePair) / 4;
    }

    //if we haven't got a match, write our old KeyValueStore struct
    if(!found)
    {
        scratchKeyValueStore(KeyValueStore(KEY_VALUE_STORAGE_MAGIC, storeSize));
        return DEVICE_NO_DATA;
    }

    //copy scratch to our storage page
    flashPageErase((uint32_t *)flashPagePtr);
    flashCopy(scratch, flashPagePtr, kvStoreSize + (storeSize * kvPairSize));

    return DEVICE_OK;
}

/**
  * Removes a KeyValuePair identified by a given key.
  *
  * @param key the unique name used to identify a KeyValuePair in flash.
  *
  * @return DEVICE_OK on success, or DEVICE_NO_DATA if the given key
  *         was not found in flash.
  */
int KeyValueStorage::remove(ManagedString key)
{
    return remove((char *)key.toCharArray());
}

/**
  * The size of the flash based KeyValueStore.
  *
  * @return the number of entries in the key value store
  */
int KeyValueStorage::size()
{
    KeyValueStore store = KeyValueStore();

    //read our data!
    memcpy(&store, flashPagePtr, sizeof(KeyValueStore));


    //if we haven't used flash before, we need to configure it
    if(store.magic != KEY_VALUE_STORAGE_MAGIC)
    {
        store.magic = KEY_VALUE_STORAGE_MAGIC;
        store.size = 0;

        //erase the scratch page and write our new KeyValueStore
        scratchKeyValueStore(store);

        //erase flash, and copy the scratch page over
        flashPageErase((uint32_t *)flashPagePtr);
        flashCopy(scratch, flashPagePtr, KEY_VALUE_STORAGE_SCRATCH_WORD_SIZE);
    }

    return store.size;
}

int KeyValueStorage::wipe()
{
    flashPageErase(this->flashPagePtr);
    size();
    return DEVICE_OK;
}