
### Instruction
* `run_tests.sh` -- script for running test 
    * TODO: only `lz4` tests are supported
* `lz4/include.cfg` -- file contains name of test-dirs for testing
    * each test-dir (for example `lz4-4k`) contains two files:
        * `*.cfg` -- contains setups for `bio_comp_dev` module
        * `*.fio` -- config for fio

> Last line of `*.cfg` should be marked
> 
>```
># END (compulsory line for test system)
>```
> * `\n` is also Ok (but **depricate**)
---
>**TODO:** If fio can't access the block device, it prints an error but the test is marked as SUCCESS.