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
#include <string>
#include <thread>
#include <vector>
#include <queue>

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
    EvalResult& eval_result
    //vector<int>& distribution,
    ) {
  // cout << "Entering safeEvaluateOpen for guesses: " << guesses[0] << endl;
  EvalResult local_result;
  std::vector<int> local_dist;
  local_dist = evaluateOpen(guesses, word_list, local_result);

  const std::lock_guard<std::mutex> lock(result_mutex);
  eval_result = local_result;
  //distribution = local_dist;
  // cout << "Leaving safeEvaluateOpen for guesses: " << guesses[0] << endl;
}

class ThreadPool {
 public:
  ThreadPool(int threads) {
    const std::lock_guard<std::mutex> lock(job_queue_mutex);
    ok_to_run = true;
    thread_idle = vector<bool>(threads, true);
    for (int i = 0; i < threads; i++) {
      pool.push_back(std::thread(&ThreadPool::loop, this, i));
    }
  }

  ~ThreadPool() {
    // Tell threads to stop.
    {
      const std::lock_guard<std::mutex> lock(job_queue_mutex);
      ok_to_run = false;
    }
    cv.notify_all();

    // Join all threads.
    for(auto& thread : pool) {
      thread.join();
    }
  }

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

  void add_job(std::function<void()> job) {
    {
      // Lock the job queue, release automatically when going out of scope.
      const std::lock_guard<std::mutex> lock(job_queue_mutex);
      job_queue.push(job);
    }
    cv.notify_one();
  }

  // Find the number of threads currently doing work.
  int workers_busy() {
    const std::lock_guard<std::mutex> lock(job_queue_mutex);
    int busy = 0;
    for (auto i : thread_idle) {
      if (!i) busy++;
    }
    return busy;
  }

  /// Return the sum the jobs in process and not started.
  int jobs_remaining() {
    const std::lock_guard<std::mutex> lock(job_queue_mutex);
    int busy = 0;
    for (auto i : thread_idle) {
      if (!i) busy++;
    }
    return busy + job_queue.size();
  }

 private:
  void loop(int id) {
    // Acquire lock.
    std::unique_lock<std::mutex> lock(job_queue_mutex);

    while(true) {
      // Check if jobs in queue, if so take one and run.
      while (!job_queue.empty()) {
        // Check if ok to run, else return.
        if (!ok_to_run) return;

        auto job = job_queue.front();
        job_queue.pop();

        // Do the work, and don't hold the lock while working.
        thread_idle[id] = false;
        lock.unlock();
        job();
        lock.lock();
        thread_idle[id] = true;
      }

      // Wait on condition variable, but check if we are still ok to run first.
      if (!ok_to_run) return;
      cv.wait(lock);
    }
  }

  std::vector<std::thread> pool;
  std::mutex job_queue_mutex;
  std::vector<bool> thread_idle;
  std::condition_variable cv;
  bool ok_to_run;
  std::queue<std::function<void()>> job_queue;
};

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

  // ==================================================================
  // Find available next guess options.
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

  // ===================================================================
  // Evaluate the opening.
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

  // =================================================================
  // Find the best single word open.
  int thread_count = 7; // TODO: set by argument
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
    EvalResult best_result = results[0];
  } else {
    cerr << "ERROR: no best open results." << endl;
  }
  for (int i = 1; i < results.size(); i++) {
    if (results[i].mean < best_result.mean) {
      best_index = i;
      best_result = results[i];
    }
  }

  cout << "Best Opening: " << answer_list[best_index] << endl;
  cout << "  with mean: " << best_result.mean << " and median: " << best_result.median << endl;

  return 0;
}