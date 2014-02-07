P1:
	- implement ffsp_rename()

	- fix garbage collection for cluster indirect data (currently disabled)

P2:
	- implement sync() to write the first erase block.

	- take all of the intelligence out of writing-into-erase-block-
	indirect-data; otherwise we get into big trouble because GC is not
	implemented for erase block indirect data. That means: for every write
	operation into (or append) an erase block indirect file, the fs
	will start a new/empty erase block - and because the write requests
	are so small this can quickly occupy all erase blocks.

P3:
	- write meta data (the first erase block) more often; it is currently
	only written on unmount.
	
	- write tests

P4:
	- implement last-write-time functionality (although there is no
	garbage collection policy that make use of it yet).

	- implement a garbage collection policy that works with the last-
	write-time instead of just looking at how full the erase blocks are.

Px:
	- code cleanup
	- documentation
	- write README
	- extend debug functions to print fs state (.FFSP file)

