#!/usr/bin/env sh

if [ -z "$CIRCLECI_TOKEN" ]; then
  echo "CIRCLECI_TOKEN env var is required"
  exit 1
fi

branch=$(git rev-parse --abbrev-ref HEAD)

pipeline_id=$(
  curl -s -u $CIRCLECI_TOKEN: -X POST https://circleci.com/api/v2/project/gh/wasmx/fizzy/pipeline \
    -H 'Content-Type: application/json' \
    -d "{\"branch\":\"$branch\", \"parameters\":{\"benchmark\":true}}" |
    jq -r '.id'
)

workflow_id=$(
  curl -s -u $CIRCLECI_TOKEN: -X GET https://circleci.com/api/v2/pipeline/$pipeline_id/workflow |
    jq -r '.items[0].id'
)

echo "workflow: https://circleci.com/workflow-run/$workflow_id"
