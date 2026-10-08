/* SPDX-License-Identifier: GPL-3.0-only */
/* FiMba-1 project: one flash object (OBJ_PROJ). A project of another format, or none (or FoMni's),
 * gives the defaults. Part of the unity build, after app.h. */
project_t proj;

void project_defaults(void)
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    for (i = 0; i < P_NPARAMS; i++)
        proj.par[i] = km_param_info(i)->def;
}

/* A project of this format loads. Each value out of its range (a setting this version added, that an older
 * project has no room for, or one whose range has changed) takes its default; the rest are kept. */
static int project_valid(int bytes)
{
    int i, have = (bytes - 8) / 2;               /* the values the saved project holds */
    if (proj.magic != PROJ_MAGIC || proj.format < 1u || proj.format > PROJ_FORMAT)
        return 0;
    if (have > PROJ_NPAR_OF(proj.format))        /* (its spare bytes, zeros, are not settings) */
        have = PROJ_NPAR_OF(proj.format);
    proj.format = PROJ_FORMAT;
    for (i = 0; i < P_NPARAMS; i++) {
        const km_param_t *p = km_param_info(i);
        if (i >= have || proj.par[i] < p->lo || proj.par[i] > p->hi)
            proj.par[i] = p->def;
    }
    return 1;
}

int project_load(void)
{
    int n;
    memset(&proj, 0, sizeof proj);
    n = plat_store_load(OBJ_PROJ, &proj, sizeof proj);
    if (n >= 8 && project_valid(n))
        return 0;
    project_defaults();
    return -1;
}

int project_save(void) { return plat_store_save(OBJ_PROJ, &proj, sizeof proj); }

void project_apply(void)
{
    int i;
    for (i = 0; i < P_NPARAMS; i++)
        km_set(i, proj.par[i]);
}
