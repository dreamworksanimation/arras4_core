#!/bin/bash

#export PATH=/rel/third_party/rez/current/bin:$PATH
export ARRAS_NODE_DIR=/usr/render_tmp

ulimit -c unlimited

#rez-env arras4_node-4.0.4 -c "arras4_node -l 5 --consul-host d_con_jose_configsvc.db.gld.dreamworks.net  --athena-env dev --use-color 0 --exclusive-user" >> /tmp/arras/arras_node.log
rez-env arras4_node -c "arras4_node --use-cgroups -l 5 --consul-host d_con_jose_configsvc.db.gld.dreamworks.net  --athena-env dev --use-color 0 --exclusive-user --no-local-rez" |& tee /tmp/arras/node_service.log
