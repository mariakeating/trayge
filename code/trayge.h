typedef struct dbus_watch_entry
{
    struct dbus_watch_entry *Next;
    struct dbus_watch_entry *Prev;
    DBusWatch *WatchHandle;
    s32 FileHandle;
    s32 PollFlags;
} dbus_watch_entry;

typedef struct dbus_timeout_entry
{
    struct dbus_timeout_entry *Next;
    struct dbus_timeout_entry *Prev;
    DBusTimeout *TimeoutHandle;
    s32 FileHandle;
} dbus_timeout_entry;

typedef struct trayge_state
{
    DBusConnection *Connection;
    const char *UniqueName;
    
    DBusObjectPathVTable Callbacks;
    
    u32 WatchCount;
    dbus_watch_entry WatchSentinel;
    
    u32 TimeoutCount;
    dbus_timeout_entry TimeoutSentinel;
    
    s32 XOffset;
    s32 YOffset;
} trayge_state;

typedef struct read_result
{
    b32 IsValid;
    u64 Count;
} read_result;

typedef enum dbus_tray_property_type
{
    DBusTrayProperty_Unhandled,
    
    DBusTrayProperty_Category,
    DBusTrayProperty_Id,
    DBusTrayProperty_Title,
    DBusTrayProperty_Status,
    //DBusTrayProperty_Menu,
    DBusTrayProperty_ItemIsMenu,
    DBusTrayProperty_IconPixmap,
    
    DBusTrayProperty_Count,
} dbus_tray_property_type;