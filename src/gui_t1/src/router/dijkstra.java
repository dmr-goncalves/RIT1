package router;
/*
 * dijkstra.java
 *
 */

import java.util.*;
//import java.net.*;
//import java.io.*;
//import javax.swing.*;
//import java.awt.event.*;


/**
 *
 * @author  Luis Bernardo
 */
public class dijkstra {
    
    protected final static int MAXINT= 2147483647;
    
    public router win;
    public int area;    // Area
    public char local_name;
    public ArrayList<NameInfo> r_map; // Cache with data since last calculation
    private Integer map_lock= new Integer(0);
    private NameInfo [] name_map; // Map between name and index
    private Integer name_map_lock= new Integer(0);
    public RouteEntry[] tab;  // Router table
    public HashMap<String,RouteEntry> tabmap;
    private Integer tab_lock= new Integer(0);
    

    public class NameInfo {
        public int n;
        public char name;
        public neighbour neig;
        public Entry[] vec;
        
        public NameInfo(int n, char name, neighbour neig) {
            this.n= n;
            this.name= name;
            this.neig= neig;
            this.vec= null;
        }
        
        public NameInfo(int n, char name, neighbour neig, Entry[] vec) {
            this.n= n;
            this.name= name;
            this.neig= neig;
            this.vec= vec;
        }
        
        public NameInfo(NameInfo src) {
            n= src.n;
            name= src.name;
            neig= src.neig;
            vec= src.vec;
        }
        
        public boolean vec_valid() {
            return (vec != null);
        } 
        
        public void setVec(Entry[] vec) {
            this.vec= vec;
        }
        
        public Entry[] getVec() {
            return vec;
        }
    }
    

    // Looks for name in table
    public int name_to_int(char name) {
        if (name_map == null)
            return -1;
        synchronized (name_map_lock) {
            for (int i= 0; i<name_map.length; i++)
                if (name == name_map[i].name)
                    return i;            
        }
        return -1;
    }
    
    // Adds a new name to the name table
    public int add_name_to_map(char name, neighbour neig, Entry[] vec) {
        int k;
        if ((k= name_to_int(name)) != -1) {
            return k;
        }
        synchronized (name_map_lock) {
            if (name_map == null)
                name_map= new NameInfo [1];
            else {
                NameInfo [] aux= new NameInfo [name_map.length+1];
                for (int i= 0; i<name_map.length; i++)
                    aux[i]= name_map[i];
                name_map= aux;
            }            
            name_map[name_map.length-1]= new NameInfo(name_map.length-1, name, 
                    neig, vec);
        }
        return name_map.length-1;
    }
    
    // Returns int translation to NameInfo
    public NameInfo int_to_nameinfo(int n) {
        if ((name_map == null) || (n >= name_map.length) || (n<0))
            return null;
        return name_map[n];
    }
    
    // Returns the char associated with an integer
    public char int_to_name(int n) {
        NameInfo pt= int_to_nameinfo(n);
        return pt==null ? ' ' : pt.name;
    }

    /** Creates a new instance of dijkstra */
    public dijkstra(router win, char local_name) {
        this.win= win;
        this.local_name= local_name;
        this.r_map= new ArrayList<NameInfo>();
        this.name_map= null;
        this.tab= null;
        this.tabmap= new HashMap<String,RouteEntry>();
    }
    
    public void clear() {
        r_map.clear();
        name_map= null;
        tab= null;
        tabmap= null;
    }

    /** Functions for preparing virtual network 
     *      Local router - neig == null;
     *      Remote router - neig points to router
     */
    public void add_router(char name, neighbour neig, Entry[] vec) {
        int n= add_name_to_map(name, neig, vec);
        if (n!=-1)
            r_map.add(name_map[n]);
    }
    
    public void add_all_vector_names() {
        Iterator it= r_map.listIterator();
        while (it.hasNext()) {
            NameInfo pt= (NameInfo)it.next();
            if (pt.vec != null) {
                for (int i= 0; i<pt.vec.length; i++)
                    add_name_to_map(pt.vec[i].dest, null, null);
            }
        }
    }
    
    /** Recalcula tabela de encaminhamento */
    public boolean run_dijkstra_algorithm(char no_inicial) {
        int n;
        int [][] matrix;
        Log2("run_dijkstra_algorithm\n");        
        // Prepare mattrix;
        synchronized (map_lock) {
            if (r_map.size() == 0)
                return false;
            add_all_vector_names(); // Create a list with all names
            n= name_map.length;
            if (n == 0)
                return false;
            // Set matrix with accessibility
            matrix= new int[n][n];
            int i;
            for (i= 0; i<n; i++) {
                for (int j= 0; j<n; j++)
                    matrix[i][j]= ((i==j) ? 0 : router.MAX_DISTANCE);
            }
            // Add all vectors to matrix
            Iterator it= r_map.listIterator();
            while (it.hasNext()) {
                NameInfo pt= (NameInfo)it.next();
                if (pt.vec_valid()) {
                    for (i= 0; i<pt.vec.length; i++) {
                        int k= name_to_int(pt.vec[i].dest);
                        if (k == -1) {
                            win.Log("Internal error 1 in Dijkstra algorithm\n");
                            return false;
                        }
                        matrix[pt.n][k]= pt.vec[i].dist;
                    }
                }
            }
        }
        int no_i= name_to_int(no_inicial);
        synchronized (tab_lock) {
            // Run Dijkstra algorithm
            tab= new RouteEntry [n];
            for (int i= 0; i<n; i++)
                tab[i]= new RouteEntry(name_map[i].name);
            if ((no_i < 0) || (no_i >= n)) {
                Log2("Error calculating Dikstra algorithm: invalid initial node\n");
                return false;
            }
            // Selects origin index
            tab[no_i].dest= no_inicial;
            tab[no_i].prox= no_inicial;
            tab[no_i].n_prox= null; // Do not send it!
            tab[no_i].ok= true;
            tab[no_i].dist= 0;
            int k= no_i;
            tabmap.put(""+no_inicial, tab[no_i]);
            //
            for (int cnt= 0; cnt<n-1; cnt++) {
                // Tests if a better path exists through k
                for (int i=0; i<n; i++) {
                    if (!tab[i].ok && (tab[i].dist > tab[k].dist + matrix[k][i])) {
                        tab[i].dist= tab[k].dist + matrix[k][i];
                        if (cnt == 0)
                            tab[i].prox= int_to_name(i);
                        else
                            tab[i].prox= tab[k].prox;
                    }
                }
                // selecciona o prox. pivot
                int m= MAXINT;
                k= 0;
                for (int i= 1; i<n; i++)
                    if (!tab[i].ok && (tab[i].dist < m)) {
                        m= tab[i].dist;
                        k= i;
                    }            
                tab[k].ok= true;
                tabmap.put(""+tab[k].dest, tab[k]);
                if (k>0) {
//                    tab[k].dest= name_map[k].name;
                    int n_prox= name_to_int(tab[k].prox);
                    if (n_prox == -1) {
                        Log2("Warning: Internal error in Dijkstra algorithm\n");
                        return false;
                    } else
                        tab[k].prox= name_map[n_prox].name;
                    tab[k].n_prox= name_map[k].neig;
                } else {
                    Log2("Warning: Internal error in Dijkstra algorithm\n");
                    return false;
                }
            }
        }
        return true;
    }

    private void Log2(String s) {
        // win.Log(s);
    }

}
