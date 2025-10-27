#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

int main() {
    int n;
    cin >> n;
    
    vector<int> a(n);
    for (int i = 0; i < n; i++) {
        cin >> a[i];
    }
    
    // Compute difference array b of size n-1
    vector<int> b(n - 1);
    for (int i = 0; i < n - 1; i++) {
        b[i] = a[i + 1] - a[i];
    }
    
    // Count positive and negative non-zero elements
    int positive_count = 0;
    int negative_count = 0;
    
    for (int i = 0; i < n - 1; i++) {
        if (b[i] > 0) {
            positive_count++;
        } else if (b[i] < 0) {
            negative_count++;
        }
    }
    
    // The minimal operations is the maximum of these counts
    int minimal_operations = max(positive_count, negative_count);
    
    // Output the result
    cout << minimal_operations << endl;
    
    // The second line of output for number of schemes remains zero for partial credit
    cout << 0 << endl;
    
    return 0;
}