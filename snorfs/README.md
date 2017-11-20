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

### Row re-map

4k region. For each of the data rows it has a sequence of bytes.
The last byte (which isn't 0xff) in sequence is the valid one.
Offsets are one off, to allow for physical row 0xff.

For every row we keep in memory after mounting:
* phys. row index (5 bits)
* first free page (8 bits)
* num. deleted pages (8 bits)


### Index pages

First 1 page after re-map. For every meta page, 8 bit hash of file name.
Special values: 0x00 - deleted, 0xff - free.

### Meta pages

These are in the first row, after indexes.

Each has:
* flags (1 byte)
* file name (63 bytes)
* file size, encoded seq. (128 bytes)
* first page: row idx + page ID (16 bit); repeated 32 times

#### File size encoding

* 0x80 - set file size to zero
* 0x80|L0 - add L0 bytes, 1 <= L0 <= 126
* 0x00|L0, 0x80|L1 = add signextend((L0<<7)|L1) bytes
* 0x00|L0, 0x00|L1, 0x80|L2 = add signextend((L0<<14)|(L1<<7)|L1) bytes

### Data row

Data row has following index data in front and back:

* for every page 8 bits for page ID; this sits in first page of row
  * 0x00 - deleted page
  * 0x01 - no page ID
  * 0xff - free page
  * other number - page ID

* for every page, 16 bit for the next page pointer (a few bits left here); this sits in last two pages of a row
  * either how many pages down in current row
  * or row idx + page ID


