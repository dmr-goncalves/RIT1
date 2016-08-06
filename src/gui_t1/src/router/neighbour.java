/**
 * Redes Integradas de Telecomunicações I
 * MIEEC 2015/2016
 *
 * neighbour.java
 *
 * Holds neighbor router internal data
 *
 * @author  Luis Bernardo
 */
package router;

import java.net.*;
import java.io.*;
import java.util.*;


public class neighbour {
       
    public char name;  // A-Z
    public String ip;
    public int port;
    public int dist;
    public InetAddress netip;
    
    // Vector-distance protocols specific data
    public Entry[] vec;     // Neighbor vector
    public Date vec_date;   // Neighbor vector reception date
    public long vec_TTL;    // TTL in seconds
    
    /** Returns the common char name */
    public char Name() { return name; }
    
    public String Ip() { return ip; }
    public int Port()  { return port; }
    public int Dist()  { return dist; }
    public InetAddress Netip() { return netip; }

    /** Vector-distance protocol specific function:
     *          Returns a vector, if it exists */
    public Entry[] Vec() { return vec_valid()? vec : null; }

    
    /** Parses a string with a compact name */
    private boolean parseName(String name) {
        // Clear name
        if (name.length() != 1)
            return false;
        char c= name.charAt(0);
        if (!Character.isUpperCase (c))
            return false;
        this.name= c;
        return true;
    }
    
    /** Creates an empty instance of neighbour */
    public neighbour() {
        clear();
    }
    
    /** Creates a new instance of neighbour */
    public neighbour(char name, String ip, int port, int distance) {
        clear();
        this.ip= ip;
        if (test_IP()) {
            this.name= name;
            this.port= port;
            this.dist= distance;
        } else
            this.ip= null;
    }
    
    /** Creates a new instance of neighbour */
    public neighbour(neighbour src) {
        this.name= src.name;
        this.ip= src.ip;
        this.netip= src.netip;
        this.port= src.port;
        this.dist= src.dist;
    }
        
    /** updates neighbour descriptor */
    public void update_neigh(char name, String ip, int port, int distance) {
        this.ip= ip;
        if (test_IP()) {
            this.name= name;
            this.port= port;
            this.dist= distance;
        } else
            clear();
    }
    
    /** Vector-distance specific function:
     *  updates last vector received from neighbor TTL in seconds
     */
    
    public void update_vec(Entry[] vec, long TTL) throws Exception {
        if (!is_valid())
            throw new Exception ("Update vector of invalid neighbor");
        this.vec= vec;
        this.vec_date= new Date();  // Now
        this.vec_TTL= TTL;
    }
    
    /** clears neighbour descriptor */
    final public void clear() {
        this.name= ' ';
        this.ip= null;
        this.netip= null;
        this.port= 0;
        this.dist= router.MAX_DISTANCE;
        this.vec= null;
        this.vec_date= null;
        this.vec_TTL= 0;
    }

    /** Test IP address */
    private boolean test_IP() {
        try {
            netip= InetAddress.getByName(ip);
            return true;
        }
        catch (UnknownHostException e) {
            netip= null;
            return false;
        }
    }

    /** Test if neighbour is valid */
    public boolean is_valid() { return (netip!=null); }
    
    /** Vector-distance protocol specific */
    public boolean vec_valid() { return (vec!=null) && ((new Date().getTime() - vec_date.getTime())<=vec_TTL*1000); }
        
    /** sends a packet to a neighbour */
    public void send_packet(DatagramSocket ds, 
                                DatagramPacket dp) throws IOException {
        try {
            dp.setAddress(this.netip);
            dp.setPort(this.port);
            ds.send(dp);
        }
        catch (IOException e) {
            throw e;
        }        
    }
    
    /** sends a packet to the neighbour */
    public void send_packet(DatagramSocket ds, 
                                ByteArrayOutputStream os) throws IOException {
        try {
            byte [] buffer = os.toByteArray();
            DatagramPacket dp= new DatagramPacket(buffer, buffer.length, 
                this.netip, this.port);
            ds.send(dp);
        }
        catch (IOException e) {
            throw e;
        }        
    }
    
    /** sends a HELLO packet to the neighbour */
    public boolean send_Hello(DatagramSocket ds, router win) {
        // Send HELLO packet
        ByteArrayOutputStream os= new ByteArrayOutputStream();
        DataOutputStream dos= new DataOutputStream(os);
        try {
            dos.writeByte(router.PKT_HELLO);
            // name ('letter')
            dos.writeChar(win.local_name());
            // Distance
            dos.writeInt(dist);
            send_packet(ds, os);
            win.HELLO_snt++;
            return true;
        }
        catch (IOException e) {
            System.out.println("Internal error sending packet HELLO: "+e+"\n");
            return false;
        }        
    }
    
    /** sends BYE packet to neighbour */
    public boolean send_Bye(DatagramSocket ds, router win) {
        ByteArrayOutputStream os= new ByteArrayOutputStream();
        DataOutputStream dos= new DataOutputStream(os);
        try {
            dos.writeByte(router.PKT_BYE);
            // name is a char
            dos.writeChar(win.local_name());
            send_packet(ds, os);
            win.BYE_snt++;
            return true;
        }
        catch (IOException e) {
            System.out.println("Internal error sending packet BYE: "+e+"\n");
            return false;
        }        
    }

    /** returns a string with the neighbour contents */
    @Override
    public String toString() {
        String str= ""+name;
        if (name == ' ')
            str= "INVALID";
        return "("+name+" ; "+ip+" ; "+port+" ; "+dist+")";
    }

    /** parses a string for a neighbour data */
    public boolean parseString(String str) {
        StringTokenizer st = new StringTokenizer(str, " ();");
        if (st.countTokens( ) != 4)
            return false;
        try {
            // Parse name
            String _name= st.nextToken();
            if (!parseName(_name))
                return false;
            if (!_name.equals(""+name))
                return false;
            String _ip= st.nextToken();
            int _port= Integer.parseInt(st.nextToken());
            int _dist= Integer.parseInt(st.nextToken());
            update_neigh(name, _ip, _port, _dist);
            return is_valid();
        }
        catch (NumberFormatException e) {
            return false;
        }
    }
}
