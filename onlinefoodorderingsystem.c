/* ONLINE FOOD ORDERING & DELIVERY SYSTEM (Console, C) - compact version
   DSA used: Linked List  -> menu, customers, cart
             Queue        -> kitchen orders (circular array, FIFO)
             Priority Q   -> delivery dispatch (Express first, array + extract-max)
             Stack        -> delivered-order history (LIFO)
             Linear Search, Merge Sort (menu by price/rating), File Handling */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <ctype.h>
#ifdef _WIN32
#include <windows.h>
#endif

#define MAX 200
#define RST "\033[0m"
#define RED "\033[31m"
#define GRN "\033[32m"
#define YEL "\033[33m"
#define CYN "\033[36m"
#define BLD "\033[1m"
#define LINE puts(CYN "--------------------------------------------------------------" RST)
#define ERR(...)  say(RED, __VA_ARGS__)
#define OK(...)   say(GRN, __VA_ARGS__)
#define INFO(...) say(YEL, __VA_ARGS__)

typedef struct Item { int id; char name[50], cat[30]; float price, rating; struct Item *next; } Item;
typedef struct Cust { int id; char name[50], phone[15], addr[100]; struct Cust *next; } Cust;
typedef struct Cart { int id, qty; char name[50]; float price; struct Cart *next; } Cart;
typedef struct { int id, custId, express; char cust[50], items[200], status[20]; float total; long time; } Order;
typedef struct { const char *label; void (*fn)(void); } Act;

static Item *menu;
static Cust *custs, *me;                 /* me = logged-in customer */
static Cart *cart;
static int nextItem = 1, nextCust = 1, nextOrder = 1, stop;
static Order kq[MAX], pq[MAX], hist[MAX];   /* kitchen queue, delivery PQ, history stack */
static int qf, qn, pqn, top = -1;

/* ------------------------------ helpers ------------------------------ */
static void say(const char *c, const char *f, ...) {
    va_list a; va_start(a, f); printf("%s", c); vprintf(f, a); printf(RST); va_end(a);
}
static char *ask(const char *p, char *out, int n) {       /* prompt + safe line input */
    char l[512]; printf("%s", p);
    if (!fgets(l, sizeof l, stdin)) l[0] = 0;
    l[strcspn(l, "\r\n")] = 0;
    snprintf(out, n, "%s", l);
    return out;
}
static int   askInt(const char *p) { char b[32]; return atoi(ask(p, b, 32)); }
static float askF(const char *p)   { char b[32]; return (float)atof(ask(p, b, 32)); }
static void updS(const char *lbl, char *d, int n) {       /* blank = keep current */
    char b[100]; printf("New %s [%s] ", lbl, d); ask("(blank = keep): ", b, sizeof b);
    if (*b) snprintf(d, n, "%s", b);
}
static void updF(const char *lbl, float *d) {
    char b[32]; printf("New %s [%.2f] ", lbl, *d); ask("(blank = keep): ", b, sizeof b);
    if (*b) *d = (float)atof(b);
}
static char *fld(char **s) {                              /* next '|' separated field */
    char *r = *s, *p; if (!r) return "";
    if ((p = strchr(r, '|'))) { *p = 0; *s = p + 1; } else *s = NULL;
    return r;
}
static int hasSub(const char *h, const char *n) {         /* case-insensitive substring */
    size_t m = strlen(n);
    for (; *h; h++) {
        size_t i = 0;
        while (i < m && h[i] && tolower((unsigned char)h[i]) == tolower((unsigned char)n[i])) i++;
        if (i == m) return 1;
    }
    return m == 0;
}
static void runMenu(const char *title, const char *zero, const Act *a, int n, int pause) {
    int c, ok; char b[8];
    do {
        printf(BLD "\n----- %s -----\n" RST, title);
        for (int i = 0; i < n; i++) printf("%d. %s\n", i + 1, a[i].label);
        printf("0. %s\n", zero);
        c = askInt("Enter choice: ");
        ok = c >= 1 && c <= n;
        if (ok) a[c - 1].fn(); else if (c) ERR("Invalid choice.\n");
        if (c && !stop && (pause || !ok)) ask("\nPress Enter to continue...", b, sizeof b);
    } while (c && !stop);
    stop = 0;
}

/* ------------------------- menu (linked list) ------------------------- */
static void appendItem(Item *n) { Item **p = &menu; while (*p) p = &(*p)->next; *p = n; }
static Item *findItem(int id)   { Item *t = menu; while (t && t->id != id) t = t->next; return t; }

static void saveMenu(void) {
    FILE *f = fopen("menu.txt", "w");
    if (!f) { ERR("Error saving menu file.\n"); return; }
    for (Item *t = menu; t; t = t->next)
        fprintf(f, "%d|%s|%s|%.2f|%.1f\n", t->id, t->name, t->cat, t->price, t->rating);
    fclose(f);
}
static void loadMenu(void) {
    FILE *f = fopen("menu.txt", "r"); char l[512];
    if (!f) return;
    while (fgets(l, sizeof l, f)) {
        l[strcspn(l, "\r\n")] = 0; if (!*l) continue;
        char *s = l; Item *n = calloc(1, sizeof *n); if (!n) continue;
        n->id = atoi(fld(&s)); snprintf(n->name, 50, "%s", fld(&s)); snprintf(n->cat, 30, "%s", fld(&s));
        n->price = (float)atof(fld(&s)); n->rating = (float)atof(fld(&s));
        appendItem(n); if (n->id >= nextItem) nextItem = n->id + 1;
    }
    fclose(f);
}
static void seedMenu(void) {
    static const struct { const char *n, *c; float p, r; } s[] = {
        {"Margherita Pizza", "Main", 249, 4.5f}, {"Veg Burger", "Main", 129, 4.2f},
        {"Chicken Biryani", "Main", 299, 4.7f},  {"Paneer Tikka", "Starter", 199, 4.3f},
        {"French Fries", "Starter", 99, 4.0f},   {"Chocolate Brownie", "Dessert", 149, 4.6f},
        {"Cold Coffee", "Beverage", 89, 4.1f},   {"Masala Dosa", "Main", 139, 4.4f} };
    if (menu) return;
    for (size_t i = 0; i < sizeof s / sizeof *s; i++) {
        Item *n = calloc(1, sizeof *n); n->id = nextItem++;
        snprintf(n->name, 50, "%s", s[i].n); snprintf(n->cat, 30, "%s", s[i].c);
        n->price = s[i].p; n->rating = s[i].r; appendItem(n);
    }
    saveMenu();
}
static void showMenu(const char *q) {                     /* q = NULL shows all; else linear search */
    int found = 0;
    printf(BLD "\n%-5s %-25s %-12s %-10s %-6s\n" RST, "ID", "Name", "Category", "Price", "Rating"); LINE;
    for (Item *t = menu; t; t = t->next)
        if (!q || hasSub(t->name, q)) {
            found = 1;
            printf("%-5d %-25s %-12s Rs%-8.2f %-6.1f\n", t->id, t->name, t->cat, t->price, t->rating);
        }
    LINE; if (!found) INFO("No items found.\n");
}
static void viewMenu(void)   { showMenu(NULL); }
static void searchMenu(void) { char q[50]; ask("Enter item name (or part of it) to search: ", q, 50); showMenu(q); }

static void addItem(void) {
    Item *n = calloc(1, sizeof *n);
    if (!n) { ERR("Memory allocation failed!\n"); return; }
    n->id = nextItem++;
    ask("Enter item name: ", n->name, 50);
    ask("Enter category (Starter/Main/Dessert/Beverage): ", n->cat, 30);
    n->price = askF("Enter price (Rs): ");
    n->rating = askF("Enter rating (0.0 - 5.0): ");
    appendItem(n); saveMenu();
    OK("\nMenu item added successfully! (ID: %d)\n", n->id);
}
static void updItem(void) {
    int id = askInt("Enter Menu Item ID to update: ");
    Item *t = findItem(id);
    if (!t) { ERR("Item with ID %d not found.\n", id); return; }
    updS("name", t->name, 50); updS("category", t->cat, 30);
    updF("price", &t->price);  updF("rating", &t->rating);
    saveMenu(); OK("Item updated successfully!\n");
}
static void delItem(void) {
    int id = askInt("Enter Menu Item ID to delete: ");
    Item **p = &menu; while (*p && (*p)->id != id) p = &(*p)->next;
    if (!*p) { ERR("Item not found.\n"); return; }
    Item *t = *p; *p = t->next; free(t);
    saveMenu(); OK("Item deleted successfully!\n");
}

/* merge sort on the linked list: mode 1 = price asc, 2 = rating desc */
static Item *mergeL(Item *a, Item *b, int mode) {
    Item d, *t = &d;
    while (a && b) {
        if (mode == 1 ? a->price <= b->price : a->rating >= b->rating) { t->next = a; a = a->next; }
        else { t->next = b; b = b->next; }
        t = t->next;
    }
    t->next = a ? a : b;
    return d.next;
}
static Item *msort(Item *h, int mode) {
    if (!h || !h->next) return h;
    Item *s = h, *f = h->next, *b;
    while (f && f->next) { s = s->next; f = f->next->next; }
    b = s->next; s->next = NULL;
    return mergeL(msort(h, mode), msort(b, mode), mode);
}
static void sortMenu(void) {
    if (!menu) { INFO("Menu is empty.\n"); return; }
    int c = askInt("Sort by: 1. Price (Low to High)   2. Rating (High to Low)\nChoice: ");
    if (c != 1 && c != 2) { ERR("Invalid choice.\n"); return; }
    menu = msort(menu, c); saveMenu();
    OK("\nMenu sorted successfully!\n"); showMenu(NULL);
}

/* ---------------------- customers (linked list) ---------------------- */
static Cust *byPhone(const char *ph) { Cust *t = custs; while (t && strcmp(t->phone, ph)) t = t->next; return t; }
static void appendCust(Cust *n)      { Cust **p = &custs; while (*p) p = &(*p)->next; *p = n; }

static void saveCusts(void) {
    FILE *f = fopen("customers.txt", "w");
    if (!f) { ERR("Error saving customers file.\n"); return; }
    for (Cust *t = custs; t; t = t->next) fprintf(f, "%d|%s|%s|%s\n", t->id, t->name, t->phone, t->addr);
    fclose(f);
}
static void loadCusts(void) {
    FILE *f = fopen("customers.txt", "r"); char l[512];
    if (!f) return;
    while (fgets(l, sizeof l, f)) {
        l[strcspn(l, "\r\n")] = 0; if (!*l) continue;
        char *s = l; Cust *n = calloc(1, sizeof *n); if (!n) continue;
        n->id = atoi(fld(&s)); snprintf(n->name, 50, "%s", fld(&s));
        snprintf(n->phone, 15, "%s", fld(&s)); snprintf(n->addr, 100, "%s", fld(&s));
        appendCust(n); if (n->id >= nextCust) nextCust = n->id + 1;
    }
    fclose(f);
}
static void showCusts(void) {
    if (!custs) { INFO("No registered customers.\n"); return; }
    printf(BLD "\n%-5s %-20s %-15s %-30s\n" RST, "ID", "Name", "Phone", "Address"); LINE;
    for (Cust *t = custs; t; t = t->next) printf("%-5d %-20s %-15s %-30s\n", t->id, t->name, t->phone, t->addr);
    LINE;
}
static void updProfile(void) {
    updS("name", me->name, 50); updS("address", me->addr, 100);
    saveCusts(); OK("Profile updated successfully!\n");
}
static void delAccount(void) {
    Cust **p = &custs; while (*p != me) p = &(*p)->next;
    *p = me->next; free(me); me = NULL; stop = 1;
    saveCusts(); OK("Account deleted successfully.\n");
}

/* ------------------------------ cart ------------------------------ */
static float cartTotal(void) { float s = 0; for (Cart *t = cart; t; t = t->next) s += t->price * t->qty; return s; }
static void clearCart(void)  { while (cart) { Cart *t = cart; cart = t->next; free(t); } }

static void addToCart(void) {
    showMenu(NULL);
    Item *it = findItem(askInt("Enter Item ID to add to cart: "));
    if (!it) { ERR("Invalid item ID.\n"); return; }
    int q = askInt("Enter quantity: ");
    if (q <= 0) { ERR("Quantity must be positive.\n"); return; }
    Cart **p = &cart; while (*p && (*p)->id != it->id) p = &(*p)->next;
    if (*p) { (*p)->qty += q; OK("Updated quantity of %s in cart to %d.\n", (*p)->name, (*p)->qty); return; }
    Cart *n = calloc(1, sizeof *n);
    n->id = it->id; snprintf(n->name, 50, "%s", it->name); n->price = it->price; n->qty = q;
    *p = n; OK("%d x %s added to cart.\n", q, it->name);
}
static void viewCart(void) {
    if (!cart) { INFO("Your cart is empty.\n"); return; }
    printf(BLD "\n%-5s %-25s %-8s %-10s %-10s\n" RST, "ID", "Item", "Qty", "Price", "Subtotal"); LINE;
    for (Cart *t = cart; t; t = t->next)
        printf("%-5d %-25s %-8d Rs%-8.2f Rs%-8.2f\n", t->id, t->name, t->qty, t->price, t->price * t->qty);
    LINE; printf(BLD "Total: Rs%.2f\n" RST, cartTotal());
}
static void removeFromCart(void) {
    if (!cart) { INFO("Cart is empty.\n"); return; }
    viewCart();
    int id = askInt("Enter Item ID to remove from cart: ");
    Cart **p = &cart; while (*p && (*p)->id != id) p = &(*p)->next;
    if (!*p) { ERR("Item not in cart.\n"); return; }
    Cart *t = *p; *p = t->next; free(t); OK("Item removed from cart.\n");
}

/* -------- kitchen queue (FIFO) / delivery priority queue / history stack -------- */
static int enqueue(Order o)   { if (qn >= MAX) return 0; kq[(qf + qn++) % MAX] = o; return 1; }
static int dequeue(Order *o)  { if (!qn) return 0; *o = kq[qf]; qf = (qf + 1) % MAX; qn--; return 1; }
static int pqExtract(Order *o) {                         /* Express first; ties -> older order id */
    if (!pqn) return 0;
    int b = 0;
    for (int i = 1; i < pqn; i++)
        if (pq[i].express > pq[b].express || (pq[i].express == pq[b].express && pq[i].id < pq[b].id)) b = i;
    *o = pq[b];
    memmove(&pq[b], &pq[b + 1], (pqn - b - 1) * sizeof(Order));
    pqn--;
    return 1;
}
static void push(Order o) { if (top >= MAX - 1) { ERR("History stack full!\n"); return; } hist[++top] = o; }

static void checkout(void) {
    if (!cart) { INFO("Your cart is empty.\n"); return; }
    viewCart();
    Order o = {0};
    o.express = askInt("\nDelivery type:\n  1. Normal\n  2. Express (+Rs30, priority delivery)\nChoice: ") == 2;
    o.id = nextOrder++; o.custId = me->id; snprintf(o.cust, 50, "%s", me->name);
    for (Cart *t = cart; t; t = t->next) {
        char s[80]; snprintf(s, 80, "%s x%d, ", t->name, t->qty);
        strncat(o.items, s, sizeof o.items - strlen(o.items) - 1);
    }
    o.total = cartTotal() + (o.express ? 30 : 0);
    strcpy(o.status, "Pending"); o.time = (long)time(NULL);
    if (!enqueue(o)) { ERR("Order queue is full. Please try again later.\n"); nextOrder--; return; }
    clearCart();
    OK("\nOrder placed successfully! Your Order ID is %d.\n", o.id);
    printf("Total Amount: Rs%.2f | Type: %s\n", o.total, o.express ? "Express" : "Normal");
}
static void kitchen(void) {
    Order o;
    if (pqn >= MAX) { ERR("Delivery queue full!\n"); return; }
    if (!dequeue(&o)) { INFO("No pending orders in the kitchen queue.\n"); return; }
    say(CYN, "\nPreparing Order #%d for %s...\n", o.id, o.cust);
    printf("Items: %s\n", o.items);
    strcpy(o.status, "Ready for Delivery"); pq[pqn++] = o;    /* insert into delivery PQ */
    OK("Order #%d is ready and sent to delivery dispatch (%s priority).\n", o.id, o.express ? "EXPRESS" : "normal");
}
static void dispatch(void) {
    Order o;
    if (!pqExtract(&o)) { INFO("No orders ready for delivery.\n"); return; }
    say(CYN, "\nDispatching Order #%d (%s) to %s...\n", o.id, o.express ? "EXPRESS" : "Normal", o.cust);
    strcpy(o.status, "Delivered"); push(o);
    FILE *f = fopen("order_history.txt", "a");
    if (f) {
        fprintf(f, "%d|%d|%s|%s|%.2f|%d|%s|%ld\n", o.id, o.custId, o.cust, o.items, o.total, o.express, o.status, o.time);
        fclose(f);
    }
    OK("Order #%d delivered successfully! Total: Rs%.2f\n", o.id, o.total);
}
static void showHist(void) {
    if (top < 0) { INFO("No delivered orders yet.\n"); return; }
    printf(BLD "\n===== ORDER HISTORY (Most Recent First) =====\n" RST);
    printf(BLD "%-6s %-18s %-10s %-10s %-30s\n" RST, "OrdID", "Customer", "Type", "Amount", "Items"); LINE;
    for (int i = top; i >= 0; i--)
        printf("%-6d %-18s %-10s Rs%-8.2f %-30s\n", hist[i].id, hist[i].cust,
               hist[i].express ? "Express" : "Normal", hist[i].total, hist[i].items);
    LINE;
}
static void loadHist(void) {
    FILE *f = fopen("order_history.txt", "r"); char l[512];
    if (!f) return;
    while (fgets(l, sizeof l, f)) {
        l[strcspn(l, "\r\n")] = 0; if (!*l) continue;
        char *s = l; Order o = {0};
        o.id = atoi(fld(&s)); o.custId = atoi(fld(&s));
        snprintf(o.cust, 50, "%s", fld(&s)); snprintf(o.items, 200, "%s", fld(&s));
        o.total = (float)atof(fld(&s)); o.express = atoi(fld(&s));
        snprintf(o.status, 20, "%s", fld(&s)); o.time = atol(fld(&s));
        push(o); if (o.id >= nextOrder) nextOrder = o.id + 1;
    }
    fclose(f);
}

/* ------------------------------ screens ------------------------------ */
static void customerArea(void) {
    static const Act a[] = { {"View Full Menu", viewMenu}, {"Search Menu Item", searchMenu},
        {"View Sorted Menu (Price/Rating)", sortMenu}, {"Add Item to Cart", addToCart},
        {"View Cart", viewCart}, {"Remove Item from Cart", removeFromCart},
        {"Checkout / Place Order", checkout}, {"Update My Profile", updProfile},
        {"Delete My Account", delAccount} };
    char t[80]; snprintf(t, sizeof t, "CUSTOMER MENU (%s)", me->name);
    runMenu(t, "Logout", a, 9, 1);
    me = NULL; clearCart();
}
static void login(void) {
    char ph[15]; ask("Enter registered phone number: ", ph, 15);
    if (!(me = byPhone(ph))) { INFO("No account found with this number. Please register first.\n"); return; }
    OK("Login successful! Welcome back, %s.\n", me->name);
    customerArea();
}
static void registerCust(void) {
    char ph[15]; ask("Enter phone number: ", ph, 15);
    if (byPhone(ph)) { ERR("A customer with this phone number already exists. Please login.\n"); return; }
    Cust *n = calloc(1, sizeof *n); n->id = nextCust++; strcpy(n->phone, ph);
    ask("Enter your name: ", n->name, 50); ask("Enter delivery address: ", n->addr, 100);
    appendCust(n); saveCusts(); me = n;
    OK("\nRegistration successful! Welcome, %s.\n", n->name);
    customerArea();
}
static void adminArea(void) {
    static const Act a[] = { {"Add Menu Item", addItem}, {"Update Menu Item", updItem},
        {"Delete Menu Item", delItem}, {"View Full Menu", viewMenu}, {"View All Customers", showCusts},
        {"Process Next Kitchen Order (Queue)", kitchen}, {"Dispatch Next Delivery (Priority Queue)", dispatch},
        {"View Order History (Stack)", showHist}, {"Sort Menu", sortMenu} };
    char p[20]; ask("Enter admin password: ", p, 20);
    if (strcmp(p, "admin123")) { ERR("Incorrect password. Access denied.\n"); return; }
    runMenu("ADMIN PANEL", "Back to Main Menu", a, 9, 1);
}

int main(void) {
    static const Act a[] = { {"Customer Login", login}, {"New Customer Registration", registerCust},
                             {"Admin Panel", adminArea} };
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE); DWORD m;
    if (GetConsoleMode(h, &m)) SetConsoleMode(h, m | 0x0004);   /* enable ANSI colours */
#endif
    loadMenu(); seedMenu(); loadCusts(); loadHist();
    runMenu("ONLINE FOOD ORDERING & DELIVERY SYSTEM", "Exit", a, 3, 0);
    puts(GRN "\nThank you for using our system. Goodbye!" RST);
    return 0;
}