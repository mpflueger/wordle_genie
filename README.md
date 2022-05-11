# wordle_genie
A tool for evaluating Wordle openings and finding the best openings.

## Usage
```
./wordle_genie [List of guesses] [answer]
```

`answer` can be the actual answer, or any string compatible with the hints seen for the guess list.

First, wordle_genie will find all the valid answers compatible with the hints that would have been given so for for the supplied answer on the supplied guesses.

Second, wordle_genie will evaluate the quality of the guesses as an opening by evaluating them against all possible answers, and providing the distribution of the number of valid answers that would have been left given the hints from all possible answers.

Example:
```
> ./wordle_genie trace lions abbey
Reading answer_list from wordle_answers.txt
  Read in 2315 words.
Running numOptions for [trace, lions, ] on abbey.
  numOptions=3 computed in 0.000267171 s
Options are:
kebab
abbey
decay
Running evaluateOpen for [trace, lions, ].
Found distribuiton in 0.58287 s:
0:0, 1:463, 2:324, 3:219, 4:200, 5:170, 6:114, 7:70, 8:152, 9:99, 10:120, 11:66, 12:36, 13:39, 14:56, 15:15, 16:0, 17:0, 18:36, 19:19, 20:0, 21:21, 22:44, 23:23, 24:0, 25:0, 26:0, 27:0, 28:0, 29:29, 30:0, 31:0, 32:0, 33:0, 34:0, 35:0, 36:0, 37:0, 38:0, 39:0, 40:0, 41:0, 42:0, 43:0, 44:0, 45:0, 46:0, 47:0, 48:0, 49:0, 50:0,
  Mean: 6.20346  Median: 4
```