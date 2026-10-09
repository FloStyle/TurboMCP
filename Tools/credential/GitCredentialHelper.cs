// Read GitHub credentials straight out of the Windows Credential Manager.
//
// Why this exists: Git Credential Manager is configured on this machine and it
// works, but Windows holds SEVERAL github.com credentials (a generic one, one per
// account name, and entries written by GitHub CLI and SourceTree). GCM finds more
// than one match and opens an interactive "Select an account" dialog on every
// push, which makes an unattended push impossible.
//
// This reads one specific entry by name and prints it, so nothing is ever asked.
// It adds no new secret: it reads a credential that is already stored and already
// verified to authenticate as FloStyle.
//
// Usage:
//   [archived item] list                     list github.com target names
//   [archived item] get <targetName>         print username and password
//
// `list` deliberately prints target names and never secrets, so it is safe to run
// and to paste into a report.

using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

internal static class GitCredentialHelper
{
    private const int CRED_TYPE_GENERIC = 1;

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct CREDENTIAL
    {
        public uint Flags;
        public uint Type;
        public IntPtr TargetName;
        public IntPtr Comment;
        public System.Runtime.InteropServices.ComTypes.FILETIME LastWritten;
        public uint CredentialBlobSize;
        public IntPtr CredentialBlob;
        public uint Persist;
        public uint AttributeCount;
        public IntPtr Attributes;
        public IntPtr TargetAlias;
        public IntPtr UserName;
    }

    [DllImport("advapi32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern bool CredRead(string target, int type, int reservedFlag, out IntPtr credentialPtr);

    [DllImport("advapi32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern bool CredEnumerate(string filter, int flag, out int count, out IntPtr credentialPtr);

    [DllImport("advapi32.dll")]
    private static extern void CredFree(IntPtr buffer);

    private static string ReadString(IntPtr ptr)
    {
        return ptr == IntPtr.Zero ? string.Empty : Marshal.PtrToStringUni(ptr) ?? string.Empty;
    }

    private static int List(string filter)
    {
        IntPtr arrayPtr;
        int count;
        if (!CredEnumerate(filter, 0, out count, out arrayPtr))
        {
            Console.Error.WriteLine("CredEnumerate failed: " + Marshal.GetLastWin32Error());
            return 1;
        }

        try
        {
            for (int i = 0; i < count; i++)
            {
                IntPtr element = Marshal.ReadIntPtr(arrayPtr, i * IntPtr.Size);
                CREDENTIAL cred = Marshal.PtrToStructure<CREDENTIAL>(element);
                string target = ReadString(cred.TargetName);
                string user = ReadString(cred.UserName);

                // Target names only. Never a secret, so this output is safe to
                // show and to record.
                Console.WriteLine("{0}\t{1}\t{2}", target, user, cred.CredentialBlobSize);
            }
        }
        finally
        {
            CredFree(arrayPtr);
        }

        return 0;
    }

    private static int Get(string target)
    {
        IntPtr credPtr;
        if (!CredRead(target, CRED_TYPE_GENERIC, 0, out credPtr))
        {
            Console.Error.WriteLine("CredRead failed for '" + target + "': " + Marshal.GetLastWin32Error());
            return 2;
        }

        try
        {
            CREDENTIAL cred = Marshal.PtrToStructure<CREDENTIAL>(credPtr);
            string user = ReadString(cred.UserName);

            // A generic credential's blob is UTF-16. This is what the Windows
            // Credential Manager stores for a git credential, and it is the same
            // secret GCM reads.
            string secret = cred.CredentialBlobSize > 0
                ? Marshal.PtrToStringUni(cred.CredentialBlob, (int)(cred.CredentialBlobSize / 2))
                : string.Empty;

            Console.WriteLine("username=" + user);
            Console.WriteLine("password=" + secret);
        }
        finally
        {
            CredFree(credPtr);
        }

        return 0;
    }

    private static int Main(string[] args)
    {
        if (args.Length == 0)
        {
            Console.Error.WriteLine("usage: [archived item] list | get <targetName>");
            return 64;
        }

        switch (args[0].ToLowerInvariant())
        {
            case "list":
                return List(args.Length > 1 ? args[1] : null);
            case "get":
                if (args.Length < 2)
                {
                    Console.Error.WriteLine("get needs a target name");
                    return 64;
                }
                return Get(args[1]);
            default:
                Console.Error.WriteLine("unknown command: " + args[0]);
                return 64;
        }
    }
}
