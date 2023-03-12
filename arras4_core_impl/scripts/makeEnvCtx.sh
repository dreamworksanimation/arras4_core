# create a new ENV_CONTEXTS file, and use setEnvCtx to add an entry
export ENV_CONTEXTS=`mktemp --tmpdir env_context_XXXXXX`
echo "{}" > $ENV_CONTEXTS
$(dirname "$BASH_SOURCE")/setEnvCtx $*
