# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import defaultdict


def _error_message(message, feature, other_feature=None):
    message = 'runtime_enabled_features.json5: {}: {}'.format(feature, message)
    if other_feature:
        message += ': {}'.format(other_feature)
    return message


def _validate_runtime_features_graph(features):
    """
    Raises AssertionError when sanity check failed.
    @param features: a List[Dict]. See origin_trials().
    @returns None
    """
    feature_pool = {str(f['name']) for f in features}
    origin_trial_pool = {
        str(f['name'])
        for f in features if f['origin_trial_feature_name']
    }
    for f in features:
        assert not f['implied_by'] or not f['depends_on'], _error_message(
            'Only one of implied_by and depends_on is allowed', f['name'])
        for d in f['depends_on']:
            assert d in feature_pool, _error_message(
                'Depends on non-existent-feature', f['name'], d)
        for i in f['implied_by']:
            assert i in feature_pool, _error_message(
                'Implied by non-existent-feature', f['name'], i)
            assert f['origin_trial_feature_name'] or i not in origin_trial_pool, \
                _error_message(
                    'A feature must be in origin trial if implied by an origin trial feature',
                    f['name'], i)

    graph = {
        str(feature['name']): feature['depends_on'] + feature['implied_by']
        for feature in features
    }
    path = set()

    def has_cycle(vertex):
        path.add(vertex)
        for neighbor in graph[vertex]:
            if neighbor in path or has_cycle(neighbor):
                return True
        path.remove(vertex)
        return False

    for f in features:
        assert not has_cycle(str(f['name'])), _error_message(
            'Cycle found in depends_on/implied_by graph', f['name'])


def origin_trials(features):
    """
    This function returns all features that are in origin trial.
    The dependency is considered in origin trial if itself is in origin trial
    or any of its dependencies are in origin trial. Propagate dependency
    tag use DFS can find all features that are in origin trial.

    @param features: a List[Dict]. Each Dict must have keys 'name',
           'depends_on', 'implied_by' and 'origin_trial_feature_name'
           (see runtime_enabled_features.json5).
    @returns Set[str(runtime feature name)]
    """
    _validate_runtime_features_graph(features)

    origin_trials_set = set()

    graph = defaultdict(list)
    for feature in features:
        for dependency in feature['depends_on']:
            graph[dependency].append(str(feature['name']))

    def dfs(node):
        origin_trials_set.add(node)
        for dependent in graph[node]:
            if dependent not in origin_trials_set:
                dfs(dependent)

    for feature in features:
        if feature['origin_trial_feature_name']:
            dfs(str(feature['name']))

    return origin_trials_set


def browser_read_access(features):
    return [
        f for f in features if f['browser_process_read_access']
        or f['browser_process_read_write_access']
    ]


def browser_write_access(features):
    return [f for f in features if f['browser_process_read_write_access']]


def override_from_pref(features):
    return [f for f in features if f['override_from_pref']]


# The list of features we want to generate getters/setters for may contain
# duplicates, so this function will return a deduped list.
def overridable_features(features):
    combined_list = override_from_pref(features) + browser_read_access(
        features)
    seen = set()
    final_list = []
    for f in combined_list:
        if f['name'] not in seen:
            seen.add(f['name'])
            final_list.append(f)
    return final_list
