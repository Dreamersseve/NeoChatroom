#include<bits/stdc++.h>
using namespace std;
constexpr int MAXN = 500000 + 5;
int n, q;
int a[MAXN];
vector<int> g[MAXN];
bool isRing[MAXN];
int ringid[MAXN], col[MAXN], csize;
vector<int> ring;
int vis[MAXN], pre[MAXN];
// HLD
int fa[MAXN], son[MAXN], siz[MAXN], dep[MAXN], top[MAXN];
// Binary lifting
int up[20][MAXN];

void dfsRing(int u){
    vis[u] = 1;
    int v = a[u];
    if(!vis[v]){
        pre[v] = u;
        dfsRing(v);
    }else if(vis[v] == 1){
        // found a cycle
        int x = u;
        vector<int> cyc;
        while(true){
            cyc.push_back(x);
            isRing[x] = true;
            if(x == v) break;
            x = pre[x];
        }
        csize = cyc.size();
        ring = cyc;
        for(int i = 0; i < csize; i++) ringid[ring[i]] = i;
    }
    vis[u] = 2;
}

void dfs1(int u){
    siz[u] = 1;
    for(int v: g[u]){
        if(isRing[v]) continue;
        fa[v] = u;
        dep[v] = dep[u] + 1;
        col[v] = col[u];
        dfs1(v);
        siz[u] += siz[v];
        if(siz[v] > siz[son[u]]) son[u] = v;
    }
}

void dfs2(int u, int tp){
    top[u] = tp;
    if(son[u]) dfs2(son[u], tp);
    for(int v: g[u]){
        if(isRing[v] || v == son[u]) continue;
        dfs2(v, v);
    }
}

int jump(int u, int d){
    for(int k = 0; k < 20; k++) if(d & (1<<k)) u = up[k][u];
    return u;
}

int main(){
    ios::sync_with_stdio(false);
    cin.tie(NULL);

    cin >> n >> q;
    for(int i = 1; i <= n; i++){
        cin >> a[i];
        g[a[i]].push_back(i);
    }
    // find ring
    for(int i = 1; i <= n; i++) if(!vis[i]) dfsRing(i);
    // init ring roots
    for(int x: ring){
        isRing[x] = true;
        col[x] = ringid[x];
        dep[x] = 1; // so dep-1 = 0
        fa[x] = x;
    }
    // HLD on trees hanging off ring
    for(int x: ring) dfs1(x);
    for(int x: ring) dfs2(x, x);
    // binary lifting
    for(int i = 1; i <= n; i++) up[0][i] = fa[i];
    for(int k = 1; k < 20; k++){
        for(int i = 1; i <= n; i++) up[k][i] = up[k-1][ up[k-1][i] ];
    }
    
    while(q--){
        int u, v;
        cin >> u >> v;
        // compute distance to ring and entry point
        int du = dep[u] - 1;
        int dv = dep[v] - 1;
        int eu = jump(u, du);
        int ev = jump(v, dv);
        if(col[u] != col[v]){
            cout << "-1 -1\n";
            continue;
        }
        int p = col[u]; // ring index
        int posu = ringid[eu];
        int posv = ringid[ev];
        int d = (posu - posv + csize) % csize;
        
        // option1: meet at posv (s = 0)
        long long x1 = du;
        long long y1 = dv + (csize - d);
        // option2: meet at posu (s = d)
        long long x2 = du + d;
        long long y2 = dv;
        
        // choose best
        auto better = [&](pair<long long,long long> a, pair<long long,long long> b){
            long long ma = max(a.first, a.second), mb = max(b.first, b.second);
            if(ma != mb) return ma < mb;
            long long mi_a = min(a.first, a.second), mi_b = min(b.first, b.second);
            if(mi_a != mi_b) return mi_a < mi_b;
            // ensure first >= second
            long long ax = a.first, ay = a.second;
            long long bx = b.first, by = b.second;
            if(ax < ay) swap(ax, ay);
            if(bx < by) swap(bx, by);
            return ax < bx;
        };
        pair<long long,long long> A = {x1, y1}, B = {x2, y2};
        pair<long long,long long> ans = better(A,B) ? A : B;
        long long X = ans.first, Y = ans.second;
        // enforce X >= Y if interchangeable
        if(X < Y) swap(X, Y);
        cout << X << " " << Y << "\n";
    }
    return 0;
}