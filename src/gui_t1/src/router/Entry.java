/**
 * Redes Integradas de Telecomunicações I
 * MIEEC 2015/2016
 *
 * Entry.java
 *
 * Auxiliary class to hold ROUTE vector information
 *
 * @author  Luis Bernardo
 */
package router;

import java.io.*;


public class Entry {

    /** Destination */
    public char dest;
    /** Distance */
    public int dist;

    /** Creates a new instance of Entry */
    public Entry(){
        this.dest= ' ';
        this.dist= -1;
    }
    /** Creates a new instance of Entry */
    public Entry(char dest, int dist){
        this.dest= dest;
        this.dist= dist;
    }

    /** Creates a new instance of Entry */
    public Entry(Entry src){
        this.dest= src.dest;
        this.dist= src.dist;
    }

     /** Creates a new instance of Entry */
    public Entry(DataInputStream dis) throws java.io.IOException {
        readEntry(dis);
    }

    /** Updates Entry data */
    public void update(char dest, int dist) {
        this.dest= dest;
        this.dist= dist;
    }

    /** Updates path field */
    public void update_dist(int dist) {
        this.dist= dist;
    }

    /* Returns string with entry */
    @Override
    public String toString() {
        return "("+dest+" , "+dist+"])";
    }

    /** Returns true if entry is equal */
    public boolean equals_to(Entry e) {
        return (dest==e.dest) && (e.dist == dist);
    }

    /** Returns true if destination is equal */
    public boolean equals_dest(Entry e) {
        return dest == e.dest;
    }

    /** Writes Entry content to DataOutputStream */
    public void writeEntry(DataOutputStream dos) throws java.io.IOException {
        dos.writeChar(dest);
        dos.writeInt(dist);
    }

    /** Read Entry from DataInputStream */
    final public void readEntry(DataInputStream dis) throws java.io.IOException {
        dest= dis.readChar();
        if (!Character.isUpperCase(dest))
            throw new IOException("Invalid address '"+dest+"'");
        dist= dis.readInt();
        if ((dist<0) || (dist>router.MAX_DISTANCE))
            throw new IOException("Invalid distance '"+dist+"'");
    }

}
