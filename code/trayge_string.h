typedef struct string
{
    u8 *Data;
    u64 Size;
} string;

#define Str(raw) (string){(u8 *)(raw), StringLength(raw)}
#define StrLit(raw) (string){(u8 *)(raw), sizeof(raw) - 1}

function u64
StringLength(const char *A)
{
    u64 Result = 0;
    
    if(A)
    {
        char *B = (char *)A;
        for(; *B; ++B);
        Result = (u64)(B - A);
    }
    
    return Result;
}

function b32
StringsAreEqual(string A, string B, u64 Flags)
{
    b32 Result = false;
    
    if(A.Size == B.Size)
    {
        Result = true;
        
        for(u64 Index = 0;
            Index < A.Size;
            ++Index)
        {
            if(A.Data[Index] != B.Data[Index])
            {
                Result = false;
                break;
            }
        }
    }
    
    return Result;
}