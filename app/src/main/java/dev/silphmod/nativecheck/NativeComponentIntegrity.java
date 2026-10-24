package dev.silphmod.nativecheck;

import android.system.Os;
import android.system.OsConstants;
import android.system.StructStat;
import java.io.File;
import java.io.FileDescriptor;
import java.io.FileInputStream;
import java.io.IOException;
import java.security.MessageDigest;

final class NativeComponentIntegrity {
    static String hash(File file)throws Exception{
        if(!file.getAbsoluteFile().equals(file.getCanonicalFile()))throw new IOException("Native component path changed");
        MessageDigest digest=MessageDigest.getInstance("SHA-256");
        FileDescriptor descriptor=Os.open(file.getAbsolutePath(),OsConstants.O_RDONLY|OsConstants.O_CLOEXEC|OsConstants.O_NOFOLLOW,0);
        try(FileInputStream input=new FileInputStream(descriptor)){
            StructStat before=Os.fstat(descriptor);
            if(!OsConstants.S_ISREG(before.st_mode)||(before.st_mode&0022)!=0||before.st_size<=0||before.st_size>64L*1024*1024)throw new IOException("Invalid native component permissions or size");
            byte[] bytes=new byte[32768];long total=0;int count;
            while((count=input.read(bytes))!=-1){total+=count;if(total>before.st_size)throw new IOException("Native component changed during verification");digest.update(bytes,0,count);}
            StructStat after=Os.fstat(descriptor);
            if(total!=before.st_size||before.st_dev!=after.st_dev||before.st_ino!=after.st_ino||before.st_size!=after.st_size
                ||before.st_mode!=after.st_mode||before.st_mtime!=after.st_mtime||before.st_ctime!=after.st_ctime)throw new IOException("Native component changed during verification");
        }finally{Os.close(descriptor);}
        char[] alphabet="0123456789abcdef".toCharArray();byte[] value=digest.digest();char[] result=new char[value.length*2];
        for(int i=0;i<value.length;i++){result[i*2]=alphabet[(value[i]&255)>>>4];result[i*2+1]=alphabet[value[i]&15];}
        return new String(result);
    }
}
