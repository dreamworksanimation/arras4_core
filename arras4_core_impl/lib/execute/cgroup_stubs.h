// Copyright 2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

enum {
    ECGROUPNOTCOMPILED = 50000,
    ECGEOF = 50023
};

struct cgroup {
    int stub;
};

struct cgroup_controller {
    int stub;
};

struct cgroup_stat {
        char name[1];
        char value[1];
};

inline int
cgroup_init(void)
{
    return ECGROUPNOTCOMPILED;
}

inline int
cgroup_get_subsys_mount_point(const char *controller, char **mount_point)
{
    return ECGROUPNOTCOMPILED;
}

inline int
cgroup_create_cgroup(struct cgroup *cgroup, int ignore_ownership)
{
    return ECGROUPNOTCOMPILED;
}

inline int
cgroup_get_task_begin(const char *cgroup, const char *controller, void **handle,
                      pid_t *pid)
{
    return 0;
}

inline int
cgroup_get_task_end(void **handle)
{
    return ECGROUPNOTCOMPILED;
}

inline int
cgroup_attach_task_pid(struct cgroup *cgroup, pid_t tid)
{
    return ECGROUPNOTCOMPILED;
}

inline const char *
cgroup_strerror(int code)
{
    static char errorstr[] = "cgroups are disabled";
    return errorstr;
}

inline struct cgroup_controller *
cgroup_get_controller(struct cgroup *cgroup, const char *name)
{
    return nullptr;
}

inline struct cgroup_controller *
cgroup_add_controller(struct cgroup *cgroup,
                      const char *name)
{
    return nullptr;
}

inline int
cgroup_get_value_uint64(struct cgroup_controller *controller,
                        const char *name, u_int64_t *value)
{
    return ECGROUPNOTCOMPILED;
}

inline int
cgroup_get_value_int64(struct cgroup_controller *controller,
                       const char *name, int64_t *value)
{
    return ECGROUPNOTCOMPILED;
}

inline int
cgroup_add_value_int64(struct cgroup_controller *controller,
                       const char *name, int64_t value)
{
    return ECGROUPNOTCOMPILED;
}

inline int
cgroup_add_value_uint64(struct cgroup_controller *controller,
                        const char *name, u_int64_t value)
{
    return ECGROUPNOTCOMPILED;
}

inline struct cgroup *
cgroup_new_cgroup(const char *name)
{
    return nullptr;
}

inline int
cgroup_get_cgroup(struct cgroup *cgroup)
{
    return ECGROUPNOTCOMPILED;
}

inline int
cgroup_modify_cgroup(struct cgroup *cgroup)
{
    return ECGROUPNOTCOMPILED;
}

inline int
cgroup_delete_cgroup(struct cgroup *cgroup, int ignore_migration)
{
    return ECGROUPNOTCOMPILED;
}

inline void
cgroup_free(struct cgroup **cgroup)
{
}

inline int
cgroup_read_stats_begin(const char *controller, const char *path, void **handle,
                        struct cgroup_stat *stat)
{
    return ECGROUPNOTCOMPILED;
}

inline int
cgroup_read_stats_next(void **handle, struct cgroup_stat *stat)
{
    return ECGROUPNOTCOMPILED;
}

inline int
cgroup_read_stats_end(void **handle)
{
    return ECGROUPNOTCOMPILED;
}



