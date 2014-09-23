#pragma once

typedef struct {
  int    hdl;
  void  *pAddr;
  char  *pzID;
} shmhandle_t;

shmhandle_t *Util_SHM_Create(const char *pzID,void **pAddr,int size);
void         Util_SHM_Destroy(shmhandle_t *hdl);
int          Util_SHM_Map(shmhandle_t *hdl,void **pAddr,int size);
void         Util_SHM_Unmap(shmhandle_t *hdl);
