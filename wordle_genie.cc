/// Wordle Genie
/// Do fancy wordle things.
/// Author: Max Pflueger

#include <assert.h>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include <queue>

#include "thread_pool.h"

using namespace std;
using namespace std::chrono_literals;

struct Guess {
  string word;

  // regex: [BYG]{5}
  // B: letter is wrong
  // Y: letter is out of place
  // G: letter is correct
  string correct;
};

struct EvalResult {
  double mean;
  double median;
};

// Note: I tried making answer a mutable reference so we could skip allocations
// in loops, it had no impact on timing.
void composeCorrectness(const string& guess, string answer, string& correct) {
  assert(guess.size() == 5);
  correct = "BBBBB";

  for(int i = 0; i < 5; i++) {
    if (guess[i] == answer[i]) {
      // Mark this letter correct and exclude it from further consideration.
      correct[i] = 'G';
      answer[i] = '-';
    }
  }
  for(int i = 0; i < 5; i++) {
    if (correct[i] == 'B' && answer.find(guess[i]) != string::npos) {
      // Mark this letter existing and exclude it from further consideration.
      correct[i] = 'Y';
      answer[i] = '-';
    }
  }
}

// Determine if a word is consistent with a set guesses and answers.
bool isViable(const string& word, const vector<Guess>& guesses) {
  string correct = "";
  for (auto& guess : guesses) {
    composeCorrectness(guess.word, word, correct);
    if (guess.correct != correct) {
      return false;
    }
  }
  return true;
}

// Determine how many answers are consistent with the guesses.
int numOptions(
    const vector<Guess>& guesses,
    const vector<string>& word_list,
    vector<string>* options = nullptr) {
  int valid_ct = 0;
  for (auto& word : word_list) {
    if (isViable(word, guesses)) {
      valid_ct++;
      if (options) {
        options->push_back(word);
      }
    }
  }
  return valid_ct;
}

// Determine how many answers are consistent with the guesses.
int numOptions(
    const vector<string>& guesses,
    const string& answer,
    const vector<string>& word_list,
    vector<string>* options = nullptr) {
  vector<Guess> new_guesses;

  // Compose the correctness result for these guesses.
  string correct = "";
  for(auto& guess : guesses) {
    composeCorrectness(guess, answer, correct);
    new_guesses.push_back({guess, correct});
  }

  // Call numOptions.
  return numOptions(new_guesses, word_list, options);
}

// Calculate how often an open will produce 0 to 50+ word options.
vector<int> evaluateOpen(
    const vector<string>& guesses,
    const vector<string>& word_list,
    EvalResult& eval_result) {
  vector<int> distribution(51, 0);

  int word_count = 0;
  int options;
  for (auto& word : word_list) {
    options = numOptions(guesses, word, word_list);
    word_count++;
    eval_result.mean += options;

    if (options > 50) options = 50;
    distribution[options]++;
  }
  eval_result.mean = eval_result.mean / word_count;

  // Find median
  eval_result.median = -1;
  int center = word_count / 2;
  for (int i = 0; i < distribution.size(); i++) {
    if (center > distribution[i]) {
      center -= distribution[i];
    } else {
      eval_result.median = i;
      break;
    }
  }

  return distribution;
}

void safeEvaluateOpen(
    const vector<string>& guesses,
    const vector<string>& word_list,
    std::mutex& result_mutex,
    EvalResult& eval_result) {
  EvalResult local_result;
  std::vector<int> local_dist;
  local_dist = evaluateOpen(guesses, word_list, local_result);

  const std::lock_guard<std::mutex> lock(result_mutex);
  eval_result = local_result;
}


void runNumOptions(const std::vector<std::string>& guesses,
                   const std::string& answer,
                   const std::vector<std::string>& answer_list) {
  // Find available next guess options.
  // Run the guesses against the answer.
  vector<string> options;
  cout << "Running numOptions for [";
  for (auto& w : guesses) {
    cout << w << ", ";
  }
  cout << "] on " << answer << "." << endl;

  auto start = chrono::steady_clock::now();
  int result = numOptions(guesses, answer, answer_list, &options);
  auto end = chrono::steady_clock::now();
  chrono::duration<double> diff = end - start;

  cout << "  numOptions=" << result << " computed in " << diff.count() << " s" << endl;
  cout << "Options are: " << endl;
  for (auto& word : options) {
    cout << word << endl;
  }
}


void runEvaluateOpen(const std::vector<std::string>& guesses,
                     const std::vector<std::string>& answer_list) {
  // Evaluate the opening.
  cout << "Running evaluateOpen for [";
  for (auto& w : guesses) {
    cout << w << ", ";
  }
  cout << "]." << endl;

  EvalResult eval_result;
  auto start = chrono::steady_clock::now();
  auto distribution =
      evaluateOpen(guesses, answer_list, eval_result);
  auto end = chrono::steady_clock::now();
  chrono::duration<double> diff = end - start;

  cout << "Found distribuiton in " << diff.count() << " s:" << endl;
  for (int i = 0; i < distribution.size(); i++) {
    cout << i << ":" << distribution[i] << ", ";
  }
  cout << endl;
  cout << "  Mean: " << eval_result.mean
       << "  Median: " << eval_result.median << endl;
}


void runFindOpening(const std::vector<std::string>& answer_list,
                    const int thread_count) {
  // Find the best single word opening.
  ThreadPool thread_pool(thread_count);
  mutex result_mutex;
  unique_lock<mutex> result_lock(result_mutex);
  condition_variable cv;
  vector<EvalResult> results(answer_list.size());
  for (int i = 0; i < answer_list.size(); i++) {
    // evaluate open with guess
    thread_pool.add_job([&open = answer_list[i], &answer_list, &result_mutex, &result = results[i]](){
      safeEvaluateOpen({open}, answer_list, result_mutex, result);});
  }

  while (true) {
    cv.wait_for(result_lock, 500ms);
    int remaining = thread_pool.jobs_remaining();
    cout << "Thread pool has " << remaining << " jobs remaining." << endl;
    if (remaining == 0) break;
  }

  int best_index = 0;
  EvalResult best_result;
  if (results.size() > 0) {
    best_result = results[0];
  } else {
    cerr << "ERROR: no best open results." << endl;
  }
  for (int i = 1; i < results.size(); i++) {
    if (results[i].mean < best_result.mean) {
      best_index = i;
      best_result = results[i];
      cout << "Good Opening: " << answer_list[best_index] << endl;
      cout << "  with mean: " << best_result.mean << " and median: " << best_result.median << endl;
    }
  }

  cout << "Best Opening: " << answer_list[best_index] << " at index " << best_index << endl;
  cout << "  with mean: " << best_result.mean << " and median: " << best_result.median << endl;
}


int main(int argc, char** argv) {
  constexpr string_view usage = 
    "./wordle_genie <mode> [args]\n"
    "    Example usage: ./wordle_genie options trace,lions abbey\n"
    "  opening\n"
    "    Find the best opening move.\n"
    "      -t N    Number of threads (default 7)\n"
    // TODO "    -w N          Number of words to open with\n"
    // TODO "    -p word,word  Find the best opening as a continuation of the word list.\n"
    // TODO "  next <wordlist> <fake answer>\n"
    // TODO "    Find the best next word after wordlist (comma separated), with clues\n"
    // TODO "    based on the fake answer.\n"
    // TODO "      -t N    Number of threads (default 7)\n"
    "  options <wordlist> <fake answer>\n"
    "    Find all possible answers given guesses in wordlist (comma separated),\n"
    "    with clues based on the fake answer.\n"
    "  evalopen <wordlist>\n"
    "    Evaluate the quality of an opening list of (comma separated) guesses.\n";
  constexpr string_view opening = "opening";
  constexpr string_view next = "next";
  constexpr string_view options = "options";
  constexpr string_view evalopen = "evalopen";

  // Read the mode.
  string mode;
  if (argc >= 1) {
    mode = argv[1];
  } else {
    cerr << usage;
    return 0;
  }

  // Parse args based on mode.
  int thread_count = 7;
  vector<string> guesses;
  string answer;
  if (mode == opening) {
    for (int i = 2; i < argc; i++) {
      if (string(argv[i]) == "-t" && i + 1 < argc) {
        stringstream ss;
        ss.str(argv[i + 1]);
        ss >> thread_count;
        i++;
      } else {
        cerr << "Ignoring arg: " << argv[i] << endl;
      }
    }
    cout << "Calculating best single word opening." << endl;
    cout << "  Using " << thread_count << " threads." << endl;
  }
  // TODO else if (mode == next) {}
  else if (mode == options) {
    istringstream ss;
    string w;
    if (argc >= 3) {
      ss.str(argv[2]);
      while (getline(ss, w, ',')) {
        guesses.push_back(w);
      }
      answer = argv[3];
    }

    cout << "Finding possible answers for guesses [";
    for (int i = 0; i < guesses.size(); i++) {
      if (i > 0) cerr << ", ";
      cout << guesses[i];
    }
    cout << "] with fake answer \"" << answer << "\"" << endl;
  }
  else if (mode == evalopen) {
    istringstream ss;
    string w;
    if (argc >= 2) {
      ss.str(argv[2]);
      while (getline(ss, w, ',')) {
        guesses.push_back(w);
      }
    }

    cout << "Evaluating the quality of the opening: [";
    for (int i = 0; i < guesses.size(); i++) {
      if (i > 0) cerr << ", ";
      cout << guesses[i];
    }
    cout << "]" << endl;
  }
  else {
    cerr << usage;
    return 0;
  }

  // Read the word list.
  vector<string> answer_list;
  string answer_file = "wordle_answers.txt";
  cout << "Reading answer_list from " << answer_file << endl;
  ifstream f;
  f.open(answer_file);
  string line;
  while(getline(f, line)) {
    if (line.size() == 5) {
      answer_list.push_back(line);
    } else {
      cerr << "Invalid line: " << line << endl;
    }
  }
  f.close();
  cout << "  Read in " << answer_list.size() << " words." << endl;

  if (mode == options) {
    runNumOptions(guesses, answer, answer_list);
  }
  if (mode == evalopen) {
    runEvaluateOpen(guesses, answer_list);
  }
  if (mode == opening) {
    runFindOpening(answer_list, thread_count);
  }

  return 0;
}