# Serial NOR file system (SNORFS)

## Data-structures

Terminology (follows ATSAMD21):

* page - can be written independent of other pages
* row - consists of one or more pages; can be erased independently

Where needed we can introduce logical pages and rows. For example, on nRF52 bytes of
page can be written independently, so we can split the hardware 512 byte page into two.
We can always pretend rows are bigger than they are.

Sizes (num. pages in row * page size)
* ATSAMD21: 4*64
* nRF51: 1*1024
* nRF52: 8*512
* S25FL116K: 16*256; read ~1.3k/ms, write 600ms/64k or 90ms/4k 

All number are little endian.

This is specific to the 2M flash part.

Use 64k rows. There's 32 of them.

## Row

Row structure:
* 1 index page
* 253 payload pages
* 2 next-pointer pages

Bytes in index correspond to pages in row. Byte 0 corresponds to index itself, and is therefore
used otherwise - to store the row re-map ID, see below. The last two bytes correspond to 
next-pointer pages, and are thus also used differently - in the first few physical rows they
hold filesystem header.

## Meta-data rows

Index values:
* 0x00 - deleted file
* 0x01-0xfe - hash of file name
* 0xff - free metadata entry

Next-pointers are currently not used.

## Data rows

Index values:
* 0x00 - deleted data page
* 0x01 - occupied
* 0xff - free data page

Next pointers hold two bytes each for every page in the row. Again, the first two bytes, and the last
four are meaningless. The two bytes are page index and logical row index.

## Row re-map

First byte of every physical row states which logical row it contains.
The special value 0xff is used to indicate that this is a scratch physical row.

### Meta pages

These are in the first row, after indexes.

Each has:
* flags (1 byte)
* file name (up to 63 bytes; NUL-terminated)
* file size, encoded seq. (128+ bytes)
* first page: row idx + page ID (16 bit); repeated 32 times

#### File size encoding

* 0x80 - set file size to zero
* 0x80|L0 - add L0 bytes, 1 <= L0 <= 126
* 0x00|L0, 0x80|L1 = add signextend((L0<<7)|L1) bytes
* 0x00|L0, 0x00|L1, 0x80|L2 = add signextend((L0<<14)|(L1<<7)|L1) bytes


