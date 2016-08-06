/**
 * Redes Integradas de Telecomunicações I
 * MIEEC 2015/2016
 *
 * RouteEntry.java
 *
 * Auxiliary class to hold Routing table entries
 *
 * @author  Luis Bernardo
 */
package router;


public class RouteEntry extends Entry {

// Fields inherited from Entry
//    public char dest;
//    public int dist;
// New fields
    public char prox;
    public neighbour n_prox;
// Holdown algorithm specific field    
    private int holdown_cnt;
// Link State Specific field
    public boolean ok;    
    
    /** Creates a new instance of RouteEntry */
    public RouteEntry(char dest) {
        super(dest, router.MAX_DISTANCE);
        prox= ' ';        
        n_prox= null;
//        
        holdown_cnt= 0;
        ok= false;
    }

    public RouteEntry(RouteEntry src) {
        super(src);
        prox= src.prox;
        n_prox= src.n_prox;
//        
        holdown_cnt= src.holdown_cnt;
    }

    public RouteEntry(char dest, char prox, neighbour n_prox, int dist) {
        super(dest, dist);
        this.prox= prox;
        this.n_prox= n_prox;
//        
        this.holdown_cnt= 0;
    }
    
// Link State Specific field
    public void set_final() { ok= true; }
    public boolean is_final() { return ok; }
    public boolean has_prox() { return prox!=' '; }
    
// Holdown algorithm specific field
    public void set_holdown(int n) { holdown_cnt= n; }
    public void decr_holdown() { if (holdown_cnt>0) holdown_cnt--; }
    public boolean is_holdown() { return holdown_cnt>0; }
    
}
