> In your readme, detail:
> * How to start your peer, any command line arguments, and what to expect. How long does it take to synchronize, what will we see when it is synchronized
> * Tell us where your consensus code is - the part that chooses which chain to synchronize. Give us the name of the file, and the line number with a 2 sentence description of how it does it.
> * 2 sentences on how you clean up peers, give us the file name and line number for this, too

## How to start the peer
just run `./chain.out`. To compile it, run `make chain`. The known hosts are hardcoded in, sorry. If you do want to change them, it's an array of pairs of host address and port. For the chain on ember, because so much is different, there's a whole bunch of conditional compilation. To compile that, run `make comp_chain -B`. The `-B` flag is to make sure that all the files are recompiled with the `COMP_CHAIN` define. Then you can run it with `./comp_chain.out`.

## Consensus code
~~Step 1 is make sure you have good insurance to cover the therapy from looking at my code~~
The main place this happens is in the `complete_consensus()` method in `chain.cpp`, line 423.
*However*, on line 511, there is also some processing in how we make the stats requests, where we filter any peers who gave us wrong data in the past
### Description
We loop through all of the replies to find the longest chain, then for all the chains that are the same length, we count the number of peers who agree with the hash, and pick the longest one. If we ever verify and find a problem, we call `LIES()` and start over again, with all the agreeing peers blacklisted.

## "Cleaning peers"
Assuming you mean getting rid of peers that haven't gossipped in a while, that is on line 607 of `chain.cpp`.
### Description
Each peer struct keeps track of the last time it was heard from. Every time `self_check` is run, if more time has passed since then than the amount of time we allow between hearing from peers (`peer_dead_time`), we remove them from the list.

If instead you mean getting rid of peers that send bad data, that is in the `LIES` function on line 351 of `chain.cpp`.
### Description
Erases the chain, clears out any and all pending requests, marks everyone who agreed with the chain as a filthy liar, and re-runs the code to request stats from everyone.