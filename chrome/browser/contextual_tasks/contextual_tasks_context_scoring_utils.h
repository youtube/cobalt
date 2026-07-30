// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_SCORING_UTILS_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_SCORING_UTILS_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"

namespace content {
class WebContents;
}  // namespace content

namespace contextual_tasks {

// Represents a passage of text from a tab and its relevance score.
// The score is computed using the cosine similarity between the query's
// embedding and the passage's embedding.
struct ScoredPassage {
  float score = 0.0f;
  std::string text;
};

struct TabSimilarityScores {
  TabSimilarityScores();
  ~TabSimilarityScores();
  TabSimilarityScores(const TabSimilarityScores&);
  TabSimilarityScores& operator=(const TabSimilarityScores&);

  ScoredPassage best;
  ScoredPassage worst = {1.0f, ""};
};

struct QueryStateSignals {
  QueryStateSignals();
  QueryStateSignals(const QueryStateSignals&) = delete;
  QueryStateSignals& operator=(const QueryStateSignals&) = delete;
  QueryStateSignals(QueryStateSignals&&);
  QueryStateSignals& operator=(QueryStateSignals&&);
  ~QueryStateSignals();

  int query_word_count = 0;
  float query_active_tab_title_similarity = 0.0f;
  std::vector<ScoredPassage> query_active_tab_passage_similarities;

  // Multi-turn embeddings.
  std::vector<float> query_embedding;
  std::vector<std::vector<float>> conversation_thread_queries_embeddings;
  std::vector<std::vector<float>> conversation_thread_titles_embeddings;
  std::vector<float> context_tab_title_embedding;
  std::vector<std::vector<float>> context_tab_passages_embeddings;
};

struct TabSignals {
  TabSignals();
  TabSignals(const TabSignals&) = delete;
  TabSignals& operator=(const TabSignals&) = delete;
  TabSignals(TabSignals&&);
  TabSignals& operator=(TabSignals&&);
  ~TabSignals();

  base::WeakPtr<content::WebContents> web_contents;

  // TODO(b/462793437): Remove embedding_score once the migration to
  // query_candidate_tab_similarity vector (title + passages) is complete.
  std::optional<float> embedding_score;

  // Lexical signals.
  int num_query_title_matching_words = 0;

  // Similarity scores for the query and the candidate tab (title + passages).
  // TODO(b/503189770): Remove these precomputed similarity scores once the
  // migration to sending raw embeddings directly to the model is complete.
  // (Represents similarity against 1 title + N passages).
  float query_candidate_tab_title_similarity = 0.0f;
  std::vector<ScoredPassage> query_candidate_tab_passage_similarities;
  // Used for debugging/logging.
  std::optional<TabSimilarityScores> similarity_scores;

  // Similarity between the active tab title and the candidate tab title.
  float active_title_candidate_title_similarity = 0.0f;

  // Dynamic (engagement) signals.
  std::optional<base::TimeDelta> duration_since_last_active;
  std::optional<base::TimeDelta> duration_of_last_visit;

  // Multi-turn embeddings.
  std::vector<float> candidate_title_embedding;
  std::vector<std::vector<float>> candidate_passages_embeddings;
};

// Gets the score for a tab based only on the static signals.
double GetScoreWithStaticSignals(const TabSignals& signals);

// Gets the score for a tab based on all signals.
double GetScoreWithAllSignals(const TabSignals& signals);

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_SCORING_UTILS_H_
