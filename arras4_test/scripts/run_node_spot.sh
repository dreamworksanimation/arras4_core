#!/bin/bash

#export PATH=/rel/third_party/rez/current/bin:$PATH
export ARRAS_NODE_DIR=/usr/render_tmp

ulimit -c unlimited

# stb
rez-env arras4_node -c "arras4_node -l 5 --consul-host q_con_jose_configsvc.db.gld.dreamworks.net  --athena-env prod --use-color 0 --exclusive-user --no-local-rez --spot-monitor" |& tee /tmp/arras/node_service.log
# uns
#rez-env arras4_node -c "arras4_node -l 5 --consul-host d_con_jose_configsvc.db.gld.dreamworks.net  --athena-env dev --use-color 0 --exclusive-user --no-local-rez" |& tee /tmp/arras/node_service.log
