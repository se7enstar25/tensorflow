/**
 * @license
 * Copyright 2021 Google LLC. All Rights Reserved.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * =============================================================================
 */

/** For trusted parters, we want to auto-run tests and mark the PR as ready to pull
    This allows us to reduce the delay to external partners

  @param {!object}
    github enables querying for PR and also create issue using rest endpoint
    context has the commit message details in the payload
  @return {string} Returns the issue number and title
*/

const intel_action = async ({github, context}) => {
  // Add Labels - kokoro:force-run, ready to pull
  // The PR is also assigned to Mihai so it doesn't have to wait for assignment
  // Additional reviewers can be added manually based on PR contents
  const labels = ['kokoro:force-run', 'ready to pull'];
  const assignees = ['mihaimaruseac'];
  const resp_label = await github.rest.issues.addLabels({
    issue_number: context.issue.number,
    owner: context.repo.owner,
    repo: context.repo.repo,
    labels: labels
  });
  if (resp_label.status >= 400) {
    console.log(resp_label);
    throw "Error adding labels to PR";
  }
  const resp_assign = await github.rest.issues.addAssignees({
    issue_number: context.issue.number,
    owner: context.repo.owner,
    repo: context.repo.repo,
    assignees: assignees
  });
  if (resp_assign.status >= 400) {
    console.log(resp_assign);
    throw "Error adding assignee to PR";
  }
  return `PR Updated successfully with Labels: ${labels} with Assignees: ${assignees}`;
};

module.exports = {
    intel: intel_action
};
