# comment

## pathconf

```cxx
path_max = pathconf(filepath, _PC_PATH_MAX);
```

第二个参数是选项的意思，我这里选择了要查询的是`_PC_PATH_MAX`，
然后返回`filepath`所在的文件系统的 最大路径长度
