/*
The MIT License (MIT)
Copyright (c) 2021 Lancaster University.
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

#ifndef CODAL_FS_H
#define CODAL_FS_H

#include "NVMController.h"
#include "FSCache.h"
#include "CodalCompat.h"

// Configuration options.
#define CODALFS_FILENAME_LENGTH        16        
#define CODALFS_MAGIC                  "CODAL_FS_1_0"

// open() flags.
#define FS_READ     0x01
#define FS_WRITE    0x02
#define FS_CREAT    0x04
#define FS_APPEND   0x08

// seek() flags.
#define FS_SEEK_SET 0x01
#define FS_SEEK_END 0x02
#define FS_SEEK_CUR 0x04

// Status flags
#define CODALFS_STATUS_INITIALISED           0x01

// FileTable codes
#define CODALFS_UNUSED                       0xFFFF
#define CODALFS_EOF                          0xEFFF
#define CODALFS_DELETED                      0x0000

// DirectorEntry flags
#define CODALFS_DIRECTORY_ENTRY_FREE         0x8000
#define CODALFS_DIRECTORY_ENTRY_VALID        0x4000
#define CODALFS_DIRECTORY_ENTRY_DIRECTORY    0x2000
#define CODALFS_DIRECTORY_ENTRY_NEW          0xffff
#define CODALFS_DIRECTORY_ENTRY_DELETED      0x0000

// Enumeration of BLOCK TYPES
#define CODALFS_BLOCK_TYPE_FILE              1
#define CODALFS_BLOCK_TYPE_DIRECTORY         2
#define CODALFS_BLOCK_TYPE_FILETABLE         3

// OTHER CONSTANTS
#define CODALFS_INVALID_ADDRESS			  	 0xffffffff
#define CODALFS_DIRECTORY_LENGTH			 0x0f000000

namespace codal
{

	//
	// Every file in the file system has a file descriptor.
	// These are held in directory entries, using the following
	// structure.
	//
	struct DirectoryEntry
	{
		char file_name[CODALFS_FILENAME_LENGTH];    // Name of the file.
		uint16_t first_block;                       // Logical block address of the start of the file.
		uint16_t flags;                             // Status of the file.
		uint32_t length;                            // Length of the file in bytes.
	};

	//
	// A directory is a list of DirectoryEntry structures:
	//
	struct Directory
	{
		DirectoryEntry entry[0];
	};

	//
	// A FileDescriptor holds contextual information needed for each OPEN file.
	//
	struct FileDescriptor
	{
		// read/write/creat flags.
		uint16_t flags;

		// FileDescriptor id
		uint16_t id;

		// current file position, in bytes.
		uint32_t seek;

		// the current file size. n.b. this may be different to that stored in the DirectoryEntry.
		uint32_t length;

		// the logical address of the directory entry for this file. 
		uint32_t dirent;

		// the directory entry of our parent directory. 
		uint32_t directory;

		// We maintain a chain of open file descriptors. Reference to the next FileDescriptor in the chain.
		FileDescriptor *next;
	};

	/**
	  * @brief Class definition for the CODAL File system
	  *
	  * CODAL file system class. Presents a POSIX-like interface consisting of:
	  * - open()
	  * - close()
	  * - read()
	  * - write()
	  * - seek()
	  * - remove()
	  *
	  * Only a single instance shoud exist at any given time.
	  */
	class CodalFS
	{
	private:

		// Status flags
		uint32_t status;

		// The interface used for all flash read/writes
		NVMController &flash;

		//Write through cache to optimize FLASH operations
	public:
		FSCache cache;
	private:

		// Total Number of logical pages available for file data (including the file table)
		int    fileSystemSize;

		// The size of a single logical block in the file system. May be smaller than a physical page.
		uint32_t blockSize;

		// Size of the file table (blocks)
		uint16_t fileSystemTableSize;

		// Cache of the last block allocated. Used to enable round robin use of blocks.
		uint16_t lastBlockAllocated;

		// Reference to the root directory of the file system.
		DirectoryEntry *rootDirectory;

		// Chain of open files.
		FileDescriptor *openFiles;

		/**
		  * Initialize the flash storage system
		  *
		  * The file system is located dynamically, based on where the program code
		  * and code data finishes. This avoids having to allocate a fixed flash
		  * region for builds even without CodalFS.
		  *
		  * This method checks if the file system already exists, and loads it.
		  * If not, it will determines the optimal size of the file system, if necessary, and format the space
		  *
		  * @return DEVICE_OK on success, or an error code.
		  */
		int init();

		/**
		  * Attempts to detect and load an existing file system.
		  *
		  * @return DEVICE_OK on success, or DEVICE_NO_DATA if the file system could not be found.
		  */
		int load();

		/**
		  * Allocate a free logical block.
		  * A round robin algorithm is used to even out the wear on the physical device.
		  * @return NULL on error, page address on success
		  */
		uint16_t getFreeBlock();


		/**
		* Retrieve the DirectoryEntry assoiated with the given file's DIRECTORY (not the file itself).
		*
		* @param filename A fully qualified filename, from the root.
		* @return the logical address of the DirectoryEntry for the given file's directory, or NULL if no entry is found.
		*/
		uint32_t getDirectoryOf(char const * filename);

		/**
		* Retrieve the DirectoryEntry for the given filename.
		*
		* @param filename A fully or partially qualified filename.
		* @param directory The directory to search. If ommitted, the root directory will be used.
		* @return the logical address of the DirectoryEntry for the given file, or NULL if no entry is found.
		*/
		uint32_t getDirectoryEntry(char const * filename, const DirectoryEntry *directory = NULL);

		/**
		* Create a new DirectoryEntry with the given filename and flags.
		*
		* @param filename A fully or partially qualified filename.
		* @param directory The directory in which to create the entry
		* @param isDirectory true if the entry being created is itself a directory
		*
		* @return The logical address od the new DirectoryEntry for the given file, or NULL if it was not possible to allocated resources.
		*/
		uint32_t createFile(char const * filename, DirectoryEntry *directory, bool isDirectory);

		/**
		* Allocate a free DiretoryEntry in the given directory, extending and refreshing the directory block if necessary.
		*
		* @param directory The directory to add a DirectoryEntry to
		* @return The logical address of the new DirectoryEntry for the given file, or 0 if it was not possible to allocated resources.
		*/
		uint32_t createDirectoryEntry(DirectoryEntry *directory);

		/**
		* Refresh the physical page associated with the given block.
		* Any logical blocks marked for deletion on that page are recycled.
		*
		* @param block the block to recycle.
		* @param type One of CODALFS_BLOCK_TYPE_FILE, CODALFS_BLOCK_TYPE_DIRECTORY, CODALFS_BLOCK_TYPE_FILETABLE.
		* Erases and regenerates the given block, recycling any data marked for deletion.
		* @return DEVICE_OK on success.
		*/
		int recycleBlock(uint16_t block, int type = CODALFS_BLOCK_TYPE_FILE);

		/**
		* Refresh the physical pages associated with the file table.
		* Any logical blocks marked for deletion on those pages are recycled back to UNUSED.
		*
		* @return DEVICE_OK on success.
		*/
		int recycleFileTable();

		/**
		* Retrieve a memory pointer for the start of the physical memory page containing the given block.
		*
		* @param block A valid block number.
		* @return the logical address of the physical page in FLASH memory holding the given block.
		*/
		uint32_t getPage(uint16_t block);

		/**
		 * Loads the given block (if necessary) and returns a pointer to the cached RAM
		 * for the given logical address.
		 */
		uint8_t *getCachedData(uint32_t address);

		/**
		* Retrieve a memory pointer for the start of the given block.
		*
		* @param block A valid block number.
		* @return A pointer to the FLASH memory associated with the given block.
		*/
		uint32_t *getBlock(uint16_t block);

		/**
		* Pull the given logical block into cache memory (if possible).
		*
		* @param block A valid block number.
		* @return A pointer to the CacheEntry associated with the given block, or NULL if the block could not be retrieved.
		*/
		CacheEntry *getCachedBlock(uint16_t block);

		/**
		 * Determines the logical memory address associated with the start of the given block
		 * @param block A valid block number.
		 * @return the logival addresses of the start of the given block.
		 */
		uint32_t addressOfBlock(uint16_t block);

		/**
		* Retrieve the next block in a chain.
		*
		* @param block A valid block number.
		*
		* @return The block number of the next block in the file.
		*/
		uint16_t getNextFileBlock(uint16_t block);

		/**
		* Determine the logical block that contains the given address.
		*
		* @param address A valid logival address within the file system space.
		*
		* @return The block number containing the given address.
		*/
		uint16_t getBlockNumber(uint32_t address);

		/**
		* Determine the number of logical blocks required to hold the file table.
		*
		* @return The number of logical blocks required to hold the file table.
		*/
		uint16_t calculateFileTableSize();

		/*
		* Update a file table entry to a given value.
		*
		* @param block The block to update.
		* @param value The value to store in the file table.
		* @return DEVICE_OK on success.
		*/
		int fileTableWrite(uint16_t block, uint16_t value);

		/**
		* Searches the list of open files for one with the given identifier.
		*
		* @param fd A previsouly opened file identifier, as returned by open().
		* @param remove Remove the file descriptor from the list if true.
		* @return A FileDescriptor matching the given ID, or NULL if the file is not open.
		*/
		FileDescriptor* getFileDescriptor(int fd, bool remove = false);

		/**
		  * Write a given buffer to the file provided.
		  *
		  * @param file FileDescriptor of the file to write
		  * @param buffer The start of the buffer to write
		  * @param length The number of bytes to write
		  * @return The number of bytes written.
		  */
		int writeBuffer(FileDescriptor *file, uint8_t* buffer, int length);


		/**
		 * Determines if the given filename is a valid filename for use in CodalFS.
		 * valid filenames must be >0 characters in lenght, NULL temrinated and contain
		 * only printable characters.
		 *
		 * @param name The name of the file to test.
		 * @return true if the filename is valid, false otherwsie.
		 */
		bool isValidFilename(const char *name);

		/**
		 * Retrieves the value of the file table at the given index, handling any necessary caching of blocks/pages.
		 * @param index the file table index to read.
		 * @return the value of the fileTable at the given index.
		 */
		 uint16_t fileTableRead(uint16_t index);

	public:

		static CodalFS *defaultFileSystem;

		/**
		  * Constructor. Creates an instance of a CodalFS.
		  */
		CodalFS(NVMController &nav, uint32_t blockSize);

		/**
		  * Open a new file, and obtain a new file handle (int) to
		  * read/write/seek the file. The flags are:
		  *  - FS_READ : read from the file.
		  *  - FS_WRITE : write to the file.
		  *  - FS_CREAT : create a new file, if it doesn't already exist.
		  *
		  * If a file is opened that doesn't exist, and FS_CREAT isn't passed,
		  * an error is returned, otherwise the file is created.
		  *
		  * @param filename name of the file to open, must contain only printable characters.
		  * @param flags One or more of FS_READ, FS_WRITE or FS_CREAT.
		  * @return return the file handle,DEVICE_NOT_SUPPORTED if the file system has
		  *         not been initialised DEVICE_INVALID_PARAMETER if the filename is
		  *         too large, DEVICE_NO_RESOURCES if the file system is full.
		  *
		  * @code
		  * CodalFS f();
		  * int fd = f.open("test.txt", FS_WRITE|FS_CREAT);
		  * if(fd<0)
		  *    print("file open error");
		  * @endcode
		  */
		int open(char const * filename, uint32_t flags);

		/**
		 * Writes back all state associated with the given file to FLASH memory,
		 * leaving the file open.
		 *
		 * @param fd file descriptor - obtained with open().
		 * @return DEVICE_OK on success, DEVICE_NOT_SUPPORTED if the file system has not
		 *         been initialised, DEVICE_INVALID_PARAMETER if the given file handle
		 *         is invalid.
		 *
		 * @code
		 * CodalFS f();
		 * int fd = f.open("test.txt", FS_READ);
		 *
		 * ...
		 *
		 * f.flush(fd);
		 * @endcode
		 */
		int flush(int fd);

		/**
		  * Close the specified file handle.
		  * File handle resources are then made available for future open() calls.
		  *
		  * close() must be called to ensure all pending data is written back to FLASH memory.
		  *
		  * @param fd file descriptor - obtained with open().
		  * @return non-zero on success, DEVICE_NOT_SUPPORTED if the file system has not
		  *         been initialised, DEVICE_INVALID_PARAMETER if the given file handle
		  *         is invalid.
		  *
		  * @code
		  * CodalFS f();
		  * int fd = f.open("test.txt", FS_READ);
		  * if(!f.close(fd))
		  *    print("error closing file.");
		  * @endcode
		  */
		int close(int fd);

		/**
		  * Move the current position of a file handle, to be used for
		  * subsequent read/write calls.
		  *
		  * The offset modifier can be:
		  *  - FS_SEEK_SET set the absolute seek position.
		  *  - FS_SEEK_CUR set the seek position based on the current offset.
		  *  - FS_SEEK_END set the seek position from the end of the file.
		  * E.g. to seek to 2nd-to-last byte, use offset=-1.
		  *
		  * @param fd file handle, obtained with open()
		  * @param offset new offset, can be positive/negative.
		  * @param flags
		  * @return new offset position on success, DEVICE_NOT_SUPPORTED if the file system
		  *         is not intiialised, DEVICE_INVALID_PARAMETER if the flag given is invalid
		  *         or the file handle is invalid.
		  *
		  * @code
		  * CodalFS f;
		  * int fd = f.open("test.txt", FS_READ);
		  * f.seek(fd, -100, FS_SEEK_END); //100 bytes before end of file.
		  * @endcode
		  */
		int seek(int fd, int offset, uint8_t flags);

		/**
		  * Write data to the file.
		  *
		  * Write from buffer, length bytes to the current seek position.
		  * On each invocation to write, the seek position of the file handle
		  * is incremented atomically, by the number of bytes returned.
		  *
		  * @param fd File handle
		  * @param buffer the buffer from which to write data
		  * @param size number of bytes to write
		  * @return number of bytes written on success, DEVICE_NO_RESOURCES if data did
		  *         not get written to flash or the file system has not been initialised,
		  *         or this file was not opened with the FS_WRITE flag set, DEVICE_INVALID_PARAMETER
		  *         if the given file handle is invalid.
		  *
		  * @code
		  * CodalFS f();
		  * int fd = f.open("test.txt", FS_WRITE);
		  * if(f.write(fd, "hello!", 7) != 7)
		  *    print("error writing");
		  * @endcode
		  */
		int write(int fd, uint8_t* buffer, int size);

		/**
		  * Read data from the file.
		  *
		  * Read len bytes from the current seek position in the file, into the
		  * buffer. On each invocation to read, the seek position of the file
		  * handle is incremented atomically, by the number of bytes returned.
		  *
		  * @param fd File handle, obtained with open()
		  * @param buffer to store data
		  * @param size number of bytes to read
		  * @return number of bytes read on success, DEVICE_NOT_SUPPORTED if the file
		  *         system is not initialised, or this file was not opened with the
		  *         FS_READ flag set, DEVICE_INVALID_PARAMETER if the given file handle
		  *         is invalid.
		  *
		  * @code
		  * CodalFS f;
		  * int fd = f.open("read.txt", FS_READ);
		  * if(f.read(fd, buffer, 100) != 100)
		  *    print("read error");
		  * @endcode
		  */
		int read(int fd, uint8_t* buffer, int size);

		/**
		  * Remove a file from the system, and free allocated assets
		  * (including assigned blocks which are returned for use by other files).
		  *
		  * @todo the file must not already have an open file handle.
		  *
		  * @param filename name of the file to remove.
		  * @return DEVICE_OK on success, DEVICE_INVALID_PARAMETER if the given filename
		  *         does not exist, DEVICE_CANCELLED if something went wrong
		  *
		  * @code
		  * CodalFS f;
		  * if(!f.remove("file.txt"))
		  *     printf("file could not be removed");
		  * @endcode
		  */
		int remove(char const * filename);

		/**
		* Creates a new directory with the given name and location
		*
		* @param name The fully qualified name of the new directory.
		* @return DEVICE_OK on success, DEVICE_INVALID_PARAMETER if the path is invalid, or MICROBT_NO_RESOURCES if the FileSystem is full.
		*/
		int createDirectory(char const *name);

		/**
		* Initialises a new file system
		*
		* @return DEVICE_OK on success, or an error code.
		*/
		int format();
	};

}
#endif
