/// Do fancy wordle things.
/// Author: Max Pflueger

#include <assert.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

using namespace std;

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

class ThreadPool {
 public:
  TheadPool(int threads) {
    for(int i = 0; i < threads; i++) {
      pool.push_back(std::thread(loop));
    }
  }

  void add_job(std::function<void()> job) {
    // Lock the job queue, release automatically when going out of scope.
    const std::lock_guard<std::mutex> lock(job_mutex);
    jobs.push(job);
  }

 private:
  void loop() {
    while(still ok to run) {
      {
        const std::lock_guard<std::mutex> lock(job_mutex);
        // TODO Wait for job on queue.
        auto job = queue.front();
        queue.pop()
      }
      job();
    }
  }

  std::vector<std::thread> pool;
  std::mutex job_mutex;
  std::queue<std::function<void()>> jobs;
}

void worker(int& next_work, mutex& m, const vector<string>& word_list) {
  while(true) {
    
  }
}

int main(int argc, char** argv) {

  // Read the args as a list of word guesses
  vector<string> guesses;
  string answer;
  for (int i = 1; i < argc - 1; i++) {
    guesses.push_back(argv[i]);
  }
  answer = argv[argc - 1];


  // Read the word list.
  string answer_file = "wordle_answers.txt";
  cout << "Reading answer_list from " << answer_file << endl;
  ifstream f;
  f.open(answer_file);
  vector<string> answer_list;
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

  // Run the guesses against the answer.
  vector<string> options;
  // cout << "Running numOptions for TRACE LOINS on ABBEY." << endl;
  cout << "Running numOptions for [";
  for (auto& w : guesses) {
    cout << w << ", ";
  }
  cout << "] on " << answer << "." << endl;
  auto start = chrono::steady_clock::now();
  // int result = numOptions({"trace", "loins"}, "abbey", answer_list, &options);
  int result = numOptions(guesses, answer, answer_list, &options);
  auto end = chrono::steady_clock::now();
  chrono::duration<double> diff = end - start;
  cout << "  numOptions=" << result << " computed in " << diff.count() << " s" << endl;
  cout << "Options are: " << endl;
  for (auto& word : options) {
    cout << word << endl;
  }

  // Evaluate the open.
  // cout << "Running evaluateOpen for TRACE LOINS." << endl;
  cout << "Running evaluateOpen for [";
  for (auto& w : guesses) {
    cout << w << ", ";
  }
  cout << "]." << endl;
  EvalResult eval_result;
  start = chrono::steady_clock::now();
  // auto distribution =
  //     evaluateOpen({"trace", "loins"}, answer_list, eval_result);
  auto distribution =
      evaluateOpen(guesses, answer_list, eval_result);
  end = chrono::steady_clock::now();
  diff = end - start;
  cout << "Found distribuiton in " << diff.count() << " s:" << endl;
  for (int i = 0; i < distribution.size(); i++) {
    cout << i << ":" << distribution[i] << ", ";
  }
  cout << endl;
  cout << "  Mean: " << eval_result.mean
       << "  Median: " << eval_result.median << endl;

  // TODO: Find the best single word open.
  mutex worker_mutex;
  int thread_count = 7;
  for (int i = 0; i < thread_count; i++) {

  }

  return 0;
}