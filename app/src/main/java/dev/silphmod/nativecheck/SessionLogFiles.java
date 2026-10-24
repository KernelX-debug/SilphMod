package dev.silphmod.nativecheck;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.LinkedHashMap;
import java.util.List;

final class SessionLogFiles {
    static final String CURRENT="sessions";
    private static boolean accepted(File root,File folder,File file) throws IOException {
        return file.getName().matches("(?:session|native-check)-[0-9]+(?:_[a-f0-9]{8})?\\.jsonl")
            && file.isFile() && file.canRead()
            && folder.getCanonicalFile().getParentFile().equals(root.getCanonicalFile())
            && file.getCanonicalFile().getParentFile().equals(folder.getCanonicalFile());
    }
    static List<File> latest(File root,String pointer,int maximum) throws IOException {
        if(maximum<1||maximum>3)throw new IllegalArgumentException("Invalid log count");
        LinkedHashMap<String,File> found=new LinkedHashMap<>();
        for(String name:new String[]{CURRENT,"checks"}){
            File folder=new File(root,name);
            if(!folder.isDirectory()||!folder.getCanonicalFile().getParentFile().equals(root.getCanonicalFile()))continue;
            File[] entries=folder.listFiles();
            if(entries!=null)for(File file:entries)if(accepted(root,folder,file))found.put(file.getCanonicalPath(),file);
            if(pointer!=null&&!pointer.isEmpty()){
                File file=new File(pointer);
                if(accepted(root,folder,file))found.put(file.getCanonicalPath(),file);
            }
        }
        ArrayList<File> result=new ArrayList<>(found.values());
        result.sort(Comparator.comparingLong(File::lastModified).reversed().thenComparing(File::getName,Comparator.reverseOrder()));
        return new ArrayList<>(result.subList(0,Math.min(maximum,result.size())));
    }
    private SessionLogFiles() { }
}
