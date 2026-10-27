package dev.rootcatch.ugphone;

import java.io.*;

final class KecleonQueue implements AutoCloseable {
    private final File file;
    private DataOutputStream writer;
    private DataInputStream reader;
    private long count,index;
    private boolean closed;
    KecleonQueue(File directory) throws IOException {
        file=File.createTempFile("silph-kecleon-",".queue",directory);
        try{writer=new DataOutputStream(new BufferedOutputStream(new FileOutputStream(file),16384));}
        catch(IOException error){file.delete();throw error;}
    }
    void add(KecleonFeed.Candidate value) throws IOException {
        if(closed||writer==null)throw new IOException("Candidate queue is not writable");
        writer.writeUTF(value.id);writer.writeUTF(value.coordinates);writer.writeUTF(value.start);writer.writeUTF(value.end);writer.writeUTF(value.country);count++;
    }
    void ready() throws IOException {
        if(closed||writer==null)throw new IOException("Candidate queue is not writable");
        writer.close();writer=null;reader=new DataInputStream(new BufferedInputStream(new FileInputStream(file),16384));
    }
    long size(){return count;}
    KecleonFeed.Candidate next() throws IOException {
        if(closed||reader==null)throw new IOException("Candidate queue is not readable");
        if(index==count)return null;
        KecleonFeed.Candidate value=new KecleonFeed.Candidate(reader.readUTF(),reader.readUTF(),reader.readUTF(),reader.readUTF(),reader.readUTF());index++;return value;
    }
    @Override public void close(){
        if(closed)return;closed=true;
        try{if(writer!=null)writer.close();}catch(IOException ignored){}
        try{if(reader!=null)reader.close();}catch(IOException ignored){}
        writer=null;reader=null;file.delete();
    }
}
