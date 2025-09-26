#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <time.h>
#include <sys/timerfd.h>

#include <dbus/dbus.h>

#include "trayge_types.h"
#include "trayge_string.h"
#include "trayge.h"

#include <stdlib.h>
#include <stdio.h>

#define ZeroStruct(pointer) ZeroSize(pointer, sizeof(*(pointer)))

function void
ZeroSize(void *Dest, u64 Count)
{
    asm volatile("rep stosb" : "+D"(Dest), "+c"(Count) : "a"(0) : "memory");
}

function read_result
WrappedRead(s32 Handle, void *Dest, u64 Size)
{
    read_result Result = {};
    
    s64 BytesRemaining = (s64)Size;
    while(BytesRemaining > 0)
    {
        s64 Offset = (s64)Size - BytesRemaining;
        s64 ReadCount = read(Handle, (u8 *)Dest + Offset, (u64)BytesRemaining);
        if(ReadCount > 0)
        {
            BytesRemaining -= ReadCount;
        }
        else if(ReadCount == 0 || errno != EINTR)
        {
            Result.IsValid = (ReadCount == 0 || errno == EAGAIN);
            break;
        }
    }
    
    if(BytesRemaining == 0)
    {
        Result.IsValid = true;
    }
    
    Assert(BytesRemaining >= 0);
    Result.Count = Size - (u64)BytesRemaining;
    
    return Result;
}

function dbus_bool_t
HandleDBusAddWatch(DBusWatch *WatchHandle, void *UserData)
{
    dbus_bool_t Result = true;
    
    if(dbus_watch_get_enabled(WatchHandle))
    {
        trayge_state *State = UserData;
        
        dbus_watch_entry *WatchEntry = malloc(sizeof(dbus_watch_entry));
        ZeroStruct(WatchEntry);
        
        WatchEntry->Next = &State->WatchSentinel;
        WatchEntry->Prev = State->WatchSentinel.Prev;
        WatchEntry->Next->Prev = WatchEntry;
        WatchEntry->Prev->Next = WatchEntry;
        
        WatchEntry->WatchHandle = WatchHandle;
        WatchEntry->FileHandle = dbus_watch_get_unix_fd(WatchHandle);
        
        u32 WatchFlags = dbus_watch_get_flags(WatchHandle);
        if(WatchFlags & DBUS_WATCH_READABLE)
        {
            WatchEntry->PollFlags |= POLLIN;
        }
        
        if(WatchFlags & DBUS_WATCH_WRITABLE)
        {
            WatchEntry->PollFlags |= POLLOUT;
        }
        
        dbus_watch_set_data(WatchHandle, WatchEntry, 0);
    }
    
    return Result;
}

function void
HandleDBusRemoveWatch(DBusWatch *WatchHandle, void *UserData)
{
    trayge_state *State = UserData;
    dbus_watch_entry *WatchEntry = dbus_watch_get_data(WatchHandle);
    if(WatchEntry)
    {
        WatchEntry->Next->Prev = WatchEntry->Prev;
        WatchEntry->Prev->Next = WatchEntry->Next;
        
        free(WatchEntry);
        --State->WatchCount;
        
        dbus_watch_set_data(WatchHandle, 0, 0);
    }
}

function void
HandleDBusToggleWatch(DBusWatch *WatchHandle, void *UserData)
{
    if(dbus_watch_get_enabled(WatchHandle))
    {
        HandleDBusAddWatch(WatchHandle, UserData);
    }
    else
    {
        HandleDBusRemoveWatch(WatchHandle, UserData);
    }
}

function dbus_bool_t
HandleDBusAddTimeout(DBusTimeout *TimeoutHandle, void *UserData)
{
    dbus_bool_t Result = true;
    
    trayge_state *State = UserData;
    if(dbus_timeout_get_enabled(TimeoutHandle))
    {
        dbus_timeout_entry *TimeoutEntry = malloc(sizeof(dbus_timeout_entry));
        ZeroStruct(TimeoutEntry);
        
        TimeoutEntry->Next = &State->TimeoutSentinel;
        TimeoutEntry->Prev = State->TimeoutSentinel.Prev;
        TimeoutEntry->Next->Prev = TimeoutEntry;
        TimeoutEntry->Prev->Next = TimeoutEntry;
        
        TimeoutEntry->TimeoutHandle = TimeoutHandle;
        TimeoutEntry->FileHandle = timerfd_create(CLOCK_MONOTONIC, 0);
        
        s64 Nanoseconds = Million*dbus_timeout_get_interval(TimeoutHandle);
        
        struct itimerspec TimerArgument = {};
        TimerArgument.it_value.tv_sec = Nanoseconds / Billion;
        TimerArgument.it_value.tv_nsec = Nanoseconds % Billion;
        TimerArgument.it_value = TimerArgument.it_interval;
        
        timerfd_settime(TimeoutEntry->FileHandle, 0, &TimerArgument, 0);
    }
    
    return Result;
}

function void
HandleDBusRemoveTimeout(DBusTimeout *TimeoutHandle, void *UserData)
{
    trayge_state *State = UserData;
    dbus_timeout_entry *TimeoutEntry = dbus_timeout_get_data(TimeoutHandle);
    if(TimeoutEntry)
    {
        TimeoutEntry->Next->Prev = TimeoutEntry->Prev;
        TimeoutEntry->Prev->Next = TimeoutEntry->Next;
        
        close(TimeoutEntry->FileHandle);
        free(TimeoutEntry);
        --State->TimeoutCount;
    }
}

function void
HandleDBusToggleTimeout(DBusTimeout *TimeoutHandle, void *UserData)
{
    if(dbus_timeout_get_enabled(TimeoutHandle))
    {
        HandleDBusAddTimeout(TimeoutHandle, UserData);
    }
    else
    {
        HandleDBusRemoveTimeout(TimeoutHandle, UserData);
    }
}

function char *
TrayPropertyTypeToName(dbus_tray_property_type Type)
{
    char *Result = 0;
    
    switch(Type)
    {
        case DBusTrayProperty_Category:
        {
            Result = "Category";
        } break;
        
        case DBusTrayProperty_Id:
        {
            Result = "Id";
        } break;
        
        case DBusTrayProperty_Title:
        {
            Result = "Title";
        } break;
        
        case DBusTrayProperty_Status:
        {
            Result = "Status";
        } break;
        
        case DBusTrayProperty_IconThemePath:
        {
            Result = "IconThemePath";
        } break;
        
        case DBusTrayProperty_Menu:
        {
            Result = "Menu";
        } break;
        
        case DBusTrayProperty_ItemIsMenu:
        {
            Result = "ItemIsMenu";
        } break;
        
        case DBusTrayProperty_IconName:
        {
            Result = "IconName";
        } break;
        
        case DBusTrayProperty_IconPixmap:
        {
            Result = "IconPixmap";
        } break;
        
        case DBusTrayProperty_AttentionIconName:
        {
            Result = "AttentionIconName";
        } break;
        
        case DBusTrayProperty_AttentionIconPixmap:
        {
            Result = "AttentionIconPixmap";
        } break;
        
        case DBusTrayProperty_Unhandled:
        case DBusTrayProperty_Count:
        {
        } break;
    }
    
    return Result;
}

function void
AppendTrayPropertyVariant(trayge_state *State, dbus_tray_property_type Type, DBusMessageIter *Parent)
{
    DBusMessageIter Variant = {};
    
    switch(Type)
    {
        case DBusTrayProperty_Category:
        {
            DeferLoop(dbus_message_iter_open_container(Parent, DBUS_TYPE_VARIANT, "s", &Variant),
                      dbus_message_iter_close_container(Parent, &Variant))
            {
                char *Value = "ApplicationStatus";
                dbus_message_iter_append_basic(&Variant, DBUS_TYPE_STRING, &Value);
            }
        } break;
        
        case DBusTrayProperty_Id:
        {
            DeferLoop(dbus_message_iter_open_container(Parent, DBUS_TYPE_VARIANT, "s", &Variant),
                      dbus_message_iter_close_container(Parent, &Variant))
            {
                char *Value = "trayge_example";
                dbus_message_iter_append_basic(&Variant, DBUS_TYPE_STRING, &Value);
            }
        } break;
        
        case DBusTrayProperty_Title:
        {
            DeferLoop(dbus_message_iter_open_container(Parent, DBUS_TYPE_VARIANT, "s", &Variant),
                      dbus_message_iter_close_container(Parent, &Variant))
            {
                char *Value = "Trayge Example";
                dbus_message_iter_append_basic(&Variant, DBUS_TYPE_STRING, &Value);
            }
        } break;
        
        case DBusTrayProperty_Status:
        {
            DeferLoop(dbus_message_iter_open_container(Parent, DBUS_TYPE_VARIANT, "s", &Variant),
                      dbus_message_iter_close_container(Parent, &Variant))
            {
                char *Value = "Active";
                dbus_message_iter_append_basic(&Variant, DBUS_TYPE_STRING, &Value);
            }
        } break;
        
        case DBusTrayProperty_IconThemePath:
        {
            DeferLoop(dbus_message_iter_open_container(Parent, DBUS_TYPE_VARIANT, "s", &Variant),
                      dbus_message_iter_close_container(Parent, &Variant))
            {
                char *Value = "";
                dbus_message_iter_append_basic(&Variant, DBUS_TYPE_STRING, &Value);
            }
        } break;
        
        case DBusTrayProperty_Menu:
        {
            DeferLoop(dbus_message_iter_open_container(Parent, DBUS_TYPE_VARIANT, "o", &Variant),
                      dbus_message_iter_close_container(Parent, &Variant))
            {
                //char *Value = "/MenuBar";
                char *Value = "/";
                dbus_message_iter_append_basic(&Variant, DBUS_TYPE_OBJECT_PATH, &Value);
            }
        } break;
        
        case DBusTrayProperty_ItemIsMenu:
        {
            DeferLoop(dbus_message_iter_open_container(Parent, DBUS_TYPE_VARIANT, "b", &Variant),
                      dbus_message_iter_close_container(Parent, &Variant))
            {
                b32 Value = false;
                dbus_message_iter_append_basic(&Variant, DBUS_TYPE_BOOLEAN, &Value);
            }
        } break;
        
        case DBusTrayProperty_IconName:
        {
            DeferLoop(dbus_message_iter_open_container(Parent, DBUS_TYPE_VARIANT, "s", &Variant),
                      dbus_message_iter_close_container(Parent, &Variant))
            {
                char *Value = "";
                dbus_message_iter_append_basic(&Variant, DBUS_TYPE_STRING, &Value);
            }
        } break;
        
        case DBusTrayProperty_IconPixmap:
        {
            DeferLoop(dbus_message_iter_open_container(Parent, DBUS_TYPE_VARIANT, "a(iiay)", &Variant),
                      dbus_message_iter_close_container(Parent, &Variant))
            {
                s32 Width = 256;
                s32 Height = 256;
                u32 Pixels[256*256];
                
                u8 *Row = (u8 *)Pixels;
                for(s32 Y = 0;
                    Y < Height;
                    ++Y)
                {
                    u32 *Pixel = (u32 *)Row;
                    for(s32 X = 0;
                        X < Width;
                        ++X)
                    {
                        *Pixel++ = (u32)0xFF | (u32)(u8)(Y + State->YOffset) << 16 | (u32)(u8)(X + State->XOffset) << 24;
                    }
                    
                    Row += Width*4;
                }
                
                DBusMessageIter IconsArray = {};
                DeferLoop(dbus_message_iter_open_container(&Variant, DBUS_TYPE_ARRAY, "(iiay)", &IconsArray),
                          dbus_message_iter_close_container(&Variant, &IconsArray))
                {
                    DBusMessageIter IconsEntry = {};
                    DeferLoop(dbus_message_iter_open_container(&IconsArray, DBUS_TYPE_STRUCT, 0, &IconsEntry),
                              dbus_message_iter_close_container(&IconsArray, &IconsEntry))
                    {
                        dbus_message_iter_append_basic(&IconsEntry, DBUS_TYPE_INT32, &Width);
                        dbus_message_iter_append_basic(&IconsEntry, DBUS_TYPE_INT32, &Height);
                        
                        DBusMessageIter IconsEntryBytes = {};
                        DeferLoop(dbus_message_iter_open_container(&IconsEntry, DBUS_TYPE_ARRAY, "y", &IconsEntryBytes),
                                  dbus_message_iter_close_container(&IconsEntry, &IconsEntryBytes))
                        {
                            u8 *Bytes = (u8 *)Pixels;
                            dbus_message_iter_append_fixed_array(&IconsEntryBytes, DBUS_TYPE_BYTE, &Bytes, Width*Height*4);
                        }
                    }
                }
            }
            
            ++State->XOffset;
            State->YOffset += 2;
        } break;
        
        case DBusTrayProperty_AttentionIconName:
        {
            DeferLoop(dbus_message_iter_open_container(Parent, DBUS_TYPE_VARIANT, "s", &Variant),
                      dbus_message_iter_close_container(Parent, &Variant))
            {
                char *Value = "";
                dbus_message_iter_append_basic(&Variant, DBUS_TYPE_STRING, &Value);
            }
        } break;
        
        case DBusTrayProperty_AttentionIconPixmap:
        {
            DeferLoop(dbus_message_iter_open_container(Parent, DBUS_TYPE_VARIANT, "a(iiay)", &Variant),
                      dbus_message_iter_close_container(Parent, &Variant))
            {
                DBusMessageIter IconsArray = {};
                DeferLoop(dbus_message_iter_open_container(&Variant, DBUS_TYPE_ARRAY, "(iiay)", &IconsArray),
                          dbus_message_iter_close_container(&Variant, &IconsArray))
                {
                }
            }
        } break;
        
        InvalidDefaultCase;
    }
}

function DBusHandlerResult
HandleDBusMessage(DBusConnection *Connection, DBusMessage *Message, void *UserData)
{
    trayge_state *State = UserData;
    
    DBusHandlerResult Result = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    
    DBusMessageIter MessageArgs = {};
    dbus_message_iter_init(Message, &MessageArgs);
    
    const char *InterfaceRaw = dbus_message_get_interface(Message);
    string Interface = Str(InterfaceRaw);
    
    const char *NameRaw = dbus_message_get_member(Message);
    string Name = Str(NameRaw);
    
    DBusMessage *Response = dbus_message_new_method_return(Message);
    
    DBusMessageIter ResponseArgs = {};
    dbus_message_iter_init_append(Response, &ResponseArgs);
    
    printf("%s %s\n", InterfaceRaw, NameRaw);
    
    s32 MessageType = dbus_message_get_type(Message);
    if(MessageType == DBUS_MESSAGE_TYPE_METHOD_CALL)
    {
        if(StringsAreEqual(Interface, StrLit("org.freedesktop.DBus.Properties"), 0))
        {
            if(StringsAreEqual(Name, StrLit("Get"), 0))
            {
                char *RequestedInterfaceRaw = 0;
                dbus_message_iter_get_basic(&MessageArgs, &RequestedInterfaceRaw);
                string RequestedInterface = Str(RequestedInterfaceRaw);
                dbus_message_iter_next(&MessageArgs);
                
                char *RequestedPropertyRaw = 0;
                dbus_message_iter_get_basic(&MessageArgs, &RequestedPropertyRaw);
                string RequestedProperty = Str(RequestedPropertyRaw);
                dbus_message_iter_next(&MessageArgs);
                
                if(StringsAreEqual(RequestedInterface, StrLit("org.kde.StatusNotifierItem"), 0))
                {
                    dbus_tray_property_type PropertyType = DBusTrayProperty_Unhandled;
                    if(StringsAreEqual(RequestedProperty, StrLit("Category"), 0))
                    {
                        PropertyType = DBusTrayProperty_Category;
                    }
                    else if(StringsAreEqual(RequestedProperty, StrLit("Id"), 0))
                    {
                        PropertyType = DBusTrayProperty_Id;
                    }
                    else if(StringsAreEqual(RequestedProperty, StrLit("Title"), 0))
                    {
                        PropertyType = DBusTrayProperty_Title;
                    }
                    else if(StringsAreEqual(RequestedProperty, StrLit("Status"), 0))
                    {
                        PropertyType = DBusTrayProperty_Status;
                    }
                    else if(StringsAreEqual(RequestedProperty, StrLit("IconThemePath"), 0))
                    {
                        PropertyType = DBusTrayProperty_IconThemePath;
                    }
                    else if(StringsAreEqual(RequestedProperty, StrLit("Menu"), 0))
                    {
                        PropertyType = DBusTrayProperty_Menu;
                    }
                    else if(StringsAreEqual(RequestedProperty, StrLit("ItemIsMenu"), 0))
                    {
                        PropertyType = DBusTrayProperty_ItemIsMenu;
                    }
                    else if(StringsAreEqual(RequestedProperty, StrLit("IconName"), 0))
                    {
                        PropertyType = DBusTrayProperty_IconName;
                    }
                    else if(StringsAreEqual(RequestedProperty, StrLit("IconPixmap"), 0))
                    {
                        PropertyType = DBusTrayProperty_IconPixmap;
                    }
                    else if(StringsAreEqual(RequestedProperty, StrLit("AttentionIconName"), 0))
                    {
                        PropertyType = DBusTrayProperty_AttentionIconName;
                    }
                    else if(StringsAreEqual(RequestedProperty, StrLit("AttentionIconPixmap"), 0))
                    {
                        PropertyType = DBusTrayProperty_AttentionIconPixmap;
                    }
                    
                    if(PropertyType != DBusTrayProperty_Unhandled)
                    {
                        AppendTrayPropertyVariant(State, PropertyType, &ResponseArgs);
                        Result = DBUS_HANDLER_RESULT_HANDLED;
                    }
                }
            }
            else if(StringsAreEqual(Name, StrLit("GetAll"), 0))
            {
                char *RequestedInterfaceRaw = 0;
                dbus_message_iter_get_basic(&MessageArgs, &RequestedInterfaceRaw);
                string RequestedInterface = Str(RequestedInterfaceRaw);
                dbus_message_iter_next(&MessageArgs);
                
                if(StringsAreEqual(RequestedInterface, StrLit("org.kde.StatusNotifierItem"), 0))
                {
                    DBusMessageIter PropertiesArray = {};
                    DeferLoop(dbus_message_iter_open_container(&ResponseArgs, DBUS_TYPE_ARRAY, "{sv}", &PropertiesArray),
                              dbus_message_iter_close_container(&ResponseArgs, &PropertiesArray))
                    {
                        DBusMessageIter PropertyEntry = {};
                        
                        for(u32 PropertyIndex = DBusTrayProperty_Unhandled + 1;
                            PropertyIndex < DBusTrayProperty_Count;
                            ++PropertyIndex)
                        {
                            DeferLoop(dbus_message_iter_open_container(&PropertiesArray, DBUS_TYPE_DICT_ENTRY, 0, &PropertyEntry),
                                      dbus_message_iter_close_container(&PropertiesArray, &PropertyEntry))
                            {
                                char *TypeString = TrayPropertyTypeToName(PropertyIndex);
                                dbus_message_iter_append_basic(&PropertyEntry, DBUS_TYPE_STRING, &TypeString);
                                
                                AppendTrayPropertyVariant(State, PropertyIndex, &PropertyEntry);
                            }
                        }
                    }
                    
                    Result = DBUS_HANDLER_RESULT_HANDLED;
                }
            }
        }
        else if(StringsAreEqual(Interface, StrLit("org.kde.StatusNotifierItem"), 0))
        {
            Result = DBUS_HANDLER_RESULT_HANDLED;
        }
    }
    else if(MessageType == DBUS_MESSAGE_TYPE_SIGNAL)
    {
    }
    
    if(Result == DBUS_HANDLER_RESULT_HANDLED)
    {
        dbus_connection_send(Connection, Response, 0);
    }
    
    dbus_message_unref(Response);
    
    return Result;
}

int
main(int ArgumentCount, char **Arguments)
{
    trayge_state State = {};
    
    State.WatchSentinel.Next = State.WatchSentinel.Prev = &State.WatchSentinel;
    State.TimeoutSentinel.Next = State.TimeoutSentinel.Prev = &State.TimeoutSentinel;
    
    State.Connection = dbus_bus_get(DBUS_BUS_SESSION, 0);
    State.UniqueName = dbus_bus_get_unique_name(State.Connection);
    
    dbus_connection_set_watch_functions(State.Connection, HandleDBusAddWatch, HandleDBusRemoveWatch, HandleDBusToggleWatch, &State, 0);
    dbus_connection_set_timeout_functions(State.Connection, HandleDBusAddTimeout, HandleDBusRemoveTimeout, HandleDBusToggleTimeout, &State, 0);
    
    State.Callbacks.message_function = HandleDBusMessage;
    dbus_connection_register_object_path(State.Connection, "/StatusNotifierItem", &State.Callbacks, &State);
    dbus_connection_register_object_path(State.Connection, "/MenuBar", &State.Callbacks, &State);
    
    {
        DBusMessage *Request = dbus_message_new_method_call("org.kde.StatusNotifierWatcher",
                                                            "/StatusNotifierWatcher",
                                                            "org.kde.StatusNotifierWatcher",
                                                            "RegisterStatusNotifierItem");
        DBusMessageIter RequestParams = {};
        dbus_message_iter_init_append(Request, &RequestParams);
        dbus_message_iter_append_basic(&RequestParams, DBUS_TYPE_STRING, &State.UniqueName);
        
        dbus_connection_send(State.Connection, Request, 0);
        dbus_message_unref(Request);
        dbus_connection_flush(State.Connection);
    }
    
    s32 TimerHandle = timerfd_create(CLOCK_MONOTONIC, 0);
    
    s64 Nanoseconds = Billion / 120;
    
    struct itimerspec TimerArgs = {};
    TimerArgs.it_value.tv_sec = Nanoseconds / Billion;
    TimerArgs.it_value.tv_nsec = Nanoseconds % Billion;
    TimerArgs.it_interval = TimerArgs.it_value;
    
    s32 x = timerfd_settime(TimerHandle, 0, &TimerArgs, 0);
    
    while(true)
    {
        u32 PollHandleCount = 0;
        struct pollfd PollHandles[32] = {};
        
        PollHandles[PollHandleCount++] = (struct pollfd){TimerHandle, POLLIN, 0};
        
        for(dbus_watch_entry *WatchEntry = State.WatchSentinel.Next;
            WatchEntry != &State.WatchSentinel;
            WatchEntry = WatchEntry->Next)
        {
            PollHandles[PollHandleCount++] = (struct pollfd){WatchEntry->FileHandle, (s16)WatchEntry->PollFlags, 0};
        }
        
        for(dbus_timeout_entry *TimeoutEntry = State.TimeoutSentinel.Next;
            TimeoutEntry != &State.TimeoutSentinel;
            TimeoutEntry = TimeoutEntry->Next)
        {
            PollHandles[PollHandleCount++] = (struct pollfd){TimeoutEntry->FileHandle, POLLIN, 0};
        }
        
        poll(PollHandles, PollHandleCount, -1);
        
        if(PollHandles[0].revents & POLLIN)
        {
            u64 Dummy;
            WrappedRead(TimerHandle, &Dummy, sizeof(Dummy));
            
            DBusMessage *Message = dbus_message_new_signal("/StatusNotifierItem", "org.kde.StatusNotifierItem", "NewIcon");
            dbus_connection_send(State.Connection, Message, 0);
            dbus_message_unref(Message);
        }
        
        for(u32 PollIndex = 1;
            PollIndex < PollHandleCount;
            ++PollIndex)
        {
            struct pollfd *PollEntry = PollHandles + PollIndex;
            
            b32 Found = false;
            
            for(dbus_watch_entry *WatchEntry = State.WatchSentinel.Next;
                !Found && WatchEntry != &State.WatchSentinel;
                WatchEntry = WatchEntry->Next)
            {
                if(PollEntry->fd == WatchEntry->FileHandle)
                {
                    Found = true;
                    
                    if(PollEntry->revents)
                    {
                        u32 WatchFlags = 0;
                        
                        if(PollEntry->revents & POLLIN)
                        {
                            WatchFlags |= DBUS_WATCH_READABLE;
                        }
                        
                        if(PollEntry->revents & POLLOUT)
                        {
                            WatchFlags |= DBUS_WATCH_WRITABLE;
                        }
                        
                        if(PollEntry->revents & POLLERR)
                        {
                            WatchFlags |= DBUS_WATCH_ERROR;
                        }
                        
                        if(PollEntry->revents & POLLHUP)
                        {
                            WatchFlags |= DBUS_WATCH_HANGUP;
                        }
                        
                        dbus_watch_handle(WatchEntry->WatchHandle, WatchFlags);
                    }
                    
                    break;
                }
            }
            
            for(dbus_timeout_entry *TimeoutEntry = State.TimeoutSentinel.Next;
                !Found && TimeoutEntry != &State.TimeoutSentinel;
                TimeoutEntry = TimeoutEntry->Next)
            {
                if(PollEntry->fd == TimeoutEntry->FileHandle)
                {
                    Found = true;
                    
                    if(PollEntry->revents & POLLIN)
                    {
                        u64 TriggerCount = 0;
                        if(WrappedRead(TimeoutEntry->FileHandle, &TriggerCount, sizeof(TriggerCount)).Count == sizeof(TriggerCount))
                        {
                            for(u64 TriggerIndex = 0;
                                TriggerIndex < TriggerCount;
                                ++TriggerIndex)
                            {
                                dbus_timeout_handle(TimeoutEntry->TimeoutHandle);
                            }
                        }
                    }
                    
                    break;
                }
            }
        }
        
        while(dbus_connection_get_dispatch_status(State.Connection) != DBUS_DISPATCH_COMPLETE)
        {
            dbus_connection_dispatch(State.Connection);
        }
        
        if(dbus_connection_has_messages_to_send(State.Connection))
        {
            dbus_connection_flush(State.Connection);
        }
    }
    
    return 0;
}
