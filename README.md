# wordle_genie
A tool for evaluating Wordle moves and finding the best openings.

## Usage
```
./wordle_genie <mode> [args]
    Example usage: ./wordle_genie options trace,lions abbey
  opening
    Find the best opening move.
      -t N    Number of threads (default 7)
  options <wordlist> <fake answer>
    Find all possible answers given guesses in wordlist (comma separated),
    with clues based on the fake answer.
  evalopen <wordlist>
    Evaluate the quality of an opening list of (comma separated) guesses.
```

### `opening` mode
In `opening` mode wordle_genie will search for the best single word opening based on the mean number of remaining options after the first guess.  Future work could include other metrics for 'best' opening.  It is recommended that you use a number of threads similar to the number of virtual cpus on your system (or maybe 1 less to keep the system responsive).

### `options` mode
In `options` mode wordle_genie will find all the valid answers compatible with the hints that would have been given so for for the supplied answer on the supplied guesses.

`fake answer` can be the actual answer, or any string compatible with the hints seen for the guess list.

### `evalopen` mode
In `evalopen` mode wordle_genie will evaluate the quality of the guesses as an opening by evaluating them against all possible answers, and providing the distribution of the number of valid answers that would have been left given the hints from all possible answers.

Example:
```
> ./wordle_genie options trace,lions abbey
Finding possible answers for guesses [trace, lions] with fake answer "abbey"
Reading answer_list from wordle_answers.txt
  Read in 2315 words.
Running numOptions for [trace, lions, ] on abbey.
  numOptions=3 computed in 0.00022318 s
Options are:
kebab
abbey
decay

> ./wordle_genie evalopen trace,lions
Evaluating the quality of the opening: [trace, lions]
Reading answer_list from wordle_answers.txt
  Read in 2315 words.
Running evaluateOpen for [trace, lions, ].
Found distribuiton in 0.325334 s:
0:0, 1:463, 2:324, 3:219, 4:200, 5:170, 6:114, 7:70, 8:152, 9:99, 10:120, 11:66, 12:36, 13:39, 14:56, 15:15, 16:0, 17:0, 18:36, 19:19, 20:0, 21:21, 22:44, 23:23, 24:0, 25:0, 26:0, 27:0, 28:0, 29:29, 30:0, 31:0, 32:0, 33:0, 34:0, 35:0, 36:0, 37:0, 38:0, 39:0, 40:0, 41:0, 42:0, 43:0, 44:0, 45:0, 46:0, 47:0, 48:0, 49:0, 50:0,
  Mean: 6.20346  Median: 4

> ./wordle_genie opening -t 3
Calculating best single word opening.
  Using 3 threads.
Reading answer_list from wordle_answers.txt
  Read in 2315 words.
Thread pool has 2312 jobs remaining.
Thread pool has 2306 jobs remaining.
Thread pool has 2300 jobs remaining.
...
```