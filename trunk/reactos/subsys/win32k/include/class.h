#ifndef _WIN32K_CLASS_H
#define _WIN32K_CLASS_H

#define IS_ATOM(x) \
  (((ULONG_PTR)(x) > 0x0) && ((ULONG_PTR)(x) < 0x10000))

typedef struct _WNDCLASS_OBJECT
{
  UINT    cbSize;
  UINT    style;
  WNDPROC lpfnWndProcA;
  WNDPROC lpfnWndProcW;
  int     cbClsExtra;
  int     cbWndExtra;
  HANDLE  hInstance;
  HICON   hIcon;
  HCURSOR hCursor;
  HBRUSH  hbrBackground;
  UNICODE_STRING lpszMenuName;
  RTL_ATOM Atom;
  HICON   hIconSm;
  BOOL Unicode;
  BOOL Global;
  LIST_ENTRY ListEntry;
  PCHAR   ExtraData;
  /* list of windows */
  LIST_ENTRY ClassWindowsListHead;
} WNDCLASS_OBJECT, *PWNDCLASS_OBJECT;

NTSTATUS FASTCALL
InitClassImpl(VOID);

NTSTATUS FASTCALL
CleanupClassImpl(VOID);

#define ClassDereferenceObject(ClassObj) \
  ObmDereferenceObject(ClassObj)

BOOL FASTCALL
ClassReferenceClassByAtom(
   PWNDCLASS_OBJECT* Class,
   RTL_ATOM Atom,
   HINSTANCE hInstance);

BOOL FASTCALL
ClassReferenceClassByName(
   PWNDCLASS_OBJECT *Class,
   LPCWSTR ClassName,
   HINSTANCE hInstance);

BOOL FASTCALL
ClassReferenceClassByNameOrAtom(
   PWNDCLASS_OBJECT *Class,
   LPCWSTR ClassNameOrAtom,
   HINSTANCE hInstance);

struct _WINDOW_OBJECT;
ULONG FASTCALL
IntGetClassLong(struct _WINDOW_OBJECT *WindowObject, ULONG Offset, BOOL Ansi);

#endif /* _WIN32K_CLASS_H */

/* EOF */
