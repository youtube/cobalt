#! /bin/bash

BRANCH="$1"
REPO_PATH="$2"
STAGE_SERVER="$3"
STAGE_USERNAME="$4"
STAGE_BASE_PATH="$5"
STAGE_SSH_KEY="$6"

if test -d clone; then
    rm -rf clone || exit 1
fi

# checkout
hg clone "https://hg.mozilla.org/$REPO_PATH" clone || exit 1

# bundle
BUNDLE=`echo $BRANCH | tr -c '\n[:alnum:]_' '-'`.hg
( cd clone && hg bundle -t gzip -a "../$BUNDLE" ) || exit 1

# blow away the checkout to save space
rm -rf clone || exit 1

# upload to a temporary name
scp -i ~/.ssh/"$STAGE_SSH_KEY" "$BUNDLE" "$STAGE_USERNAME@$STAGE_SERVER:$STAGE_BASE_PATH/bundles/$BUNDLE.upload" || exit 1

# rename quickly to the published name (avoids partial downloads)
ssh -i ~/.ssh/"$STAGE_SSH_KEY" "$STAGE_USERNAME@$STAGE_SERVER" mv "$STAGE_BASE_PATH/bundles/$BUNDLE"{.upload,} || exit 1

# delete the bundle, too, to save space
rm $BUNDLE
