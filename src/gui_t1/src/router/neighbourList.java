/**
 * Redes Integradas de Telecomunicações I
 * MIEEC 2015/2016
 *
 * neighbourList.java
 *
 * Holds the neighbor list router internal data
 *
 * @author  Luis Bernardo
 */
package router;

import java.util.*;
import java.net.*;
import java.io.*;
import javax.swing.*;


public class neighbourList {
    
    private int max_range= 0;
    private router win;
    public HashMap<String, neighbour> list;
    final private Integer list_lock= new Integer(0);

    /** Creates a new instance of neighbourList */
    public neighbourList(int max_range, router win) {        
        this.max_range= max_range;
        this.win= win;
        list= new HashMap<String,neighbour>();
    }

    /** Returns an Iterator for scanning all members of the neighbour list */
    public Iterator<neighbour> iterator() {
        return list.values().iterator();
    }

    /** Adds a new neighbour */
    public boolean add_neig(char name, String ip, int port, int distance, DatagramSocket ds) {
        char local_name= win.local_name();        
        boolean novo;
        System.out.println("add_neig("+name+")");
        synchronized (list_lock) {
            if ((novo= !list.containsKey(""+name)) && (list.size()==max_range)) {
                System.out.println("List is full\n");
                return false;
            }
        }
        neighbour pt= locate_neig(ip, port);
        if (local_name == name) {
            System.out.println("Name equals local_name");
            return false;
        }
        if ((pt != null) && (pt.Name()!= name)) {                
            System.out.println("Duplicated IP and port\n");
            return false;
        }
        if ((distance<1) || (distance>router.MAX_DISTANCE)) {
            System.out.println("Invalid distance ("+distance+")");
            return false;
        }
        // Prepare neighbour entry
        pt= new neighbour(name, ip, port, distance);
        if (!pt.is_valid()) {
            System.out.println("Invalid neighbour data\n");
            return false;
        }
        synchronized (list_lock) {
            // Adds or replaces a member of the table
            list.put(""+name, pt);
        }
        if (novo) // If not known
            pt.send_Hello(ds, win);
        return true;
    }
        
    /** Updates the neighbour data */
    public boolean update_neig(char name, String ip, int port, int distance) {
        System.out.println("update_neig("+name+")");
        neighbour pt= locate_neig(ip, port);
        if (pt == null) {
            System.out.println("Inexistant Neighbour\n");
            return false;
        }
        if ((distance<1) || (distance>router.MAX_DISTANCE)) {
            System.out.println("Invalid distance ("+distance+")");
            return false;
        }
        if (name != pt.Name ()) {
            System.out.println("Invalid name - missmatched name previously associated with IP/port");
            return false;
        }
        if (pt.Dist() == distance) {
            // Did not change distance
            return false;
        }
        // Prepare neighbour entry
        pt.update_neigh(pt.Name(), ip, port, distance);
        return true;
    }    
    
    /** Deletes a neighbour */
    public boolean del_neig(char name, boolean send_msg, DatagramSocket ds, 
            char local_name) {
        neighbour neig= null;
        synchronized (list_lock) {
            try {
                neig= (neighbour)list.get(""+name);
            }
            catch (Exception e) {
                return false;
            }
        }
        if (neig == null) {
            win.Log("Neighbour "+name+" not deleted\n");
            return false;
        }
        if (send_msg)
            neig.send_Bye(ds, win);
        synchronized (list_lock) {
            // Adds or replaces a member of the table
            list.remove(""+name);
        }
        return true;
    }    

    /** Deletes a neighbour */
    public boolean del_neig(neighbour neig, boolean send_msg, DatagramSocket ds) {
        synchronized (list_lock) {
            if (!list.containsValue(neig))
                return false;
        }
        if (send_msg)
            neig.send_Bye(ds, win);        
        synchronized (list_lock) {
            // Removes a member from the list
            list.remove(""+neig.Name());
        }
        return true;
    }
    
    /** empty list and send BYE to all members */
    public void clear_BYE(DatagramSocket ds, char local_name) {
        synchronized (list_lock) {
            Iterator it= list.values().iterator();
            while (it.hasNext()) {
                neighbour pt= (neighbour)it.next();
                pt.send_Bye(ds, win);
            }
        }
        clear();
    }
    
    /** empty list */
    public void clear() {
        synchronized (list_lock) {
            list.clear();
        }
    }
    
    /** Locates a neighbour by name */
    public neighbour locate_neig(char name) {
        return (neighbour)list.get(""+name);
    }

    /** Locates a neighbour by ip + port */
    public neighbour locate_neig(String ip, int port) {
        synchronized (list_lock) {
            Iterator it= list.values().iterator();
            while (it.hasNext()) {
                neighbour pt= (neighbour)it.next();
                if ((ip.compareTo(pt.Ip()) == 0) && (port == pt.Port()))
                    return pt;
            }
        }
        return null;
    }

    /** Sends packet to all neighbours except 'exc' */
    public void send_packet(DatagramSocket ds, DatagramPacket dp, 
                            neighbour exc) throws IOException {
        synchronized (list_lock) {
            Iterator it= list.values().iterator();
            while (it.hasNext()) {
                neighbour pt= (neighbour)it.next();
                if (pt != exc)
                    pt.send_packet(ds, dp);
            }
        }        
    }

    /** Refreshes window Table with neighbour data */
    public boolean refresh_table(JTable table) {
        synchronized (list_lock) {
            if (table.getColumnCount() < 4)
                // Invalid number of columns
                return false;
            if (table.getRowCount() < max_range)
                // Invalid number of rows
                return false;
            
            // Update table
            Iterator it= list.values().iterator();        
            for (int i= 0; i<max_range; i++) { // For every row
                neighbour pt;
                if (it.hasNext())
                    pt= (neighbour)it.next();
                else
                    pt= null;
                if (pt == null) {
                    for (int j= 0; j<4; j++)
                        table.setValueAt("", i,  j);
                } else {
                    table.setValueAt(""+pt.Name(), i,  0);
                    table.setValueAt(pt.Ip(), i,  1);
                    table.setValueAt(""+pt.Port(), i,  2);
                    table.setValueAt(""+pt.Dist(), i,  3);
                }
            }
        }
        return true;
    }   
    
    
    /* ********************************************************************* */
    /* Functions for link state support                                      */
    /* ********************************************************************* */
    
    /** Checks for duplicate entries */
    static private boolean duplicate_entry (ArrayList<Entry> list, char name) {
        Iterator<Entry> it= list.listIterator();        
        while (it.hasNext()) {
            Entry pt= it.next();
            if (pt.dest == name)
                return true;
        }
        return false;
    }
            
    /** For link state protocols - returns the local vector
     */    
    public Entry[] local_vec(boolean add_local) {
        ArrayList<Entry> aux= new ArrayList<Entry>();
        
        if (add_local) {
            // Adds the local name
            aux.add(new Entry(win.local_name(), 0));
        }
            
        synchronized (list_lock) {            
            Iterator<neighbour> it= list.values().iterator();
            while (it.hasNext()) {
                neighbour pt= it.next();
                if (pt.is_valid()) {
                    aux.add(new Entry(pt.Name(), pt.Dist()));
                }
            }
                
            // Creates an array with all elements
            Entry[] vec= null;
            if (aux.size()>0) {
                // vec= (Entry [])aux.toArray(); did not work
                vec= new Entry[aux.size()];
                for (int i= 0; i<aux.size(); i++)
                    vec[i]= aux.get(i);
            }
            aux.clear();
            return vec;
        }        
    }
    
    /* ********************************************************************* */
    /* End of functions for link state support                               */
    /* ********************************************************************* */

}