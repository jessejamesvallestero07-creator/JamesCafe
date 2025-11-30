// main.cpp
// James' Café - Chill Coffee Shop CLI Ordering System
// Portable fixes for VS Code (g++/clang) and Visual Studio (MSVC)

#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <limits>

#ifdef _WIN32
#include <windows.h>
// Some toolchains may not define this constant; define if missing
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#endif

using namespace std;

/* -------------------- ANSI COLOR HELPERS -------------------- */
namespace Colors {
    const string RESET = "\x1B" "[0m";
    const string TITLE = "\x1B" "[1;36m"; // bright cyan
    const string SUBTLE = "\x1B" "[0;36m"; // cyan
    const string HIGHL = "\x1B" "[1;32m"; // bright green
    const string ACCENT = "\x1B" "[0;33m"; // yellow
    const string ERR = "\x1B" "[1;31m"; // red
    const string MUTED = "\x1B" "[0;37m"; // light gray
    const string SOLD_OUT = "\x1B" "[0;31m"; // regular red
}

/* -------------------- Enable ANSI on Windows (best-effort) -------------------- */
void enableAnsiOnWindows() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return;
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    // ignore failure: if it fails, we continue without colors
    SetConsoleMode(hOut, dwMode);
#endif
}

/* -------------------- Utility: Safe Input Parsers -------------------- */
string readLineTrimmed() {
    string s;
    if (!std::getline(cin, s)) return "";
    // Remove possible '\r' left by Windows CRLF
    if (!s.empty() && s.back() == '\r') s.pop_back();
    const char* ws = " \t\n\r\f\v";
    size_t start = s.find_first_not_of(ws);
    if (start == string::npos) return "";
    size_t end = s.find_last_not_of(ws);
    return s.substr(start, end - start + 1);
}

int readIntInRange(const string& prompt, int minVal, int maxVal) {
    while (true) {
        cout << prompt;
        string line;
        if (!std::getline(cin, line)) { // robust to EOF
            cin.clear();
            continue;
        }
        // trim windows CR
        if (!line.empty() && line.back() == '\r') line.pop_back();
        stringstream ss(line);
        int x;
        if (ss >> x && ss.eof()) {
            if (x < minVal || x > maxVal) {
                cout << Colors::ERR << "Please enter a number between "
                    << minVal << " and " << maxVal << "." << Colors::RESET << "\n";
                continue;
            }
            return x;
        }
        cout << Colors::ERR << "Invalid number. Try again." << Colors::RESET << "\n";
    }
}

bool readYesNo(const string& prompt) {
    while (true) {
        cout << prompt;
        string line;
        if (!std::getline(cin, line)) {
            cin.clear();
            continue;
        }
        if (!line.empty() && line.back() == '\r') line.pop_back();
        transform(line.begin(), line.end(), line.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (line == "y" || line == "yes") return true;
        if (line == "n" || line == "no") return false;
        cout << Colors::ERR << "Please answer Y or N." << Colors::RESET << "\n";
    }
}

/* -------------------- Domain Classes -------------------- */
struct Item {
    string name;
    double price;
    int qty;
    string category;
    int sold = 0;

    Item(const string& n = "", double p = 0.0, int q = 0, const string& c = "")
        : name(n), price(p), qty(q), category(c) {
    }
};

struct OrderLine {
    Item* item;
    int quantity;
    double subtotal() const { return (item ? item->price * quantity : 0.0); }
};

class Order {
public:
    string customerName;
    string dineOption;
    vector<OrderLine> lines;
    unsigned long long receiptNo = 0;
    chrono::system_clock::time_point timestamp;

    Order() {
        timestamp = chrono::system_clock::now();
    }

    double total() const {
        double t = 0.0;
        for (const auto& l : lines) t += l.subtotal();
        return t;
    }

    void printReceipt() const {
        time_t tt = chrono::system_clock::to_time_t(timestamp);

        // portable localtime handling
        tm local_tm{};
#ifdef _WIN32
        localtime_s(&local_tm, &tt);
#else
        localtime_r(&tt, &local_tm);
#endif

        char timebuf[64];
        strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", &local_tm);

        cout << Colors::TITLE << "\n=== James' Café Receipt ===" << Colors::RESET << "\n";
        cout << Colors::SUBTLE << "Receipt# " << receiptNo << "     " << timebuf << Colors::RESET << "\n";
        cout << Colors::MUTED << "Customer: " << customerName << "     (" << dineOption << ")" << Colors::RESET << "\n\n";
        cout << left << setw(30) << "Item" << setw(6) << "Qty" << setw(12) << "Subtotal" << "\n";
        cout << "-----------------------------------------------\n";
        for (const auto& l : lines) {
            cout << left << setw(30) << (l.item ? l.item->name : string("(unknown)"))
                << setw(6) << l.quantity
                << "₱ " << fixed << setprecision(2) << l.subtotal() << "\n";
        }
        cout << "-----------------------------------------------\n";
        cout << Colors::HIGHL << "TOTAL: ₱ " << fixed << setprecision(2) << total() << Colors::RESET << "\n";
        cout << Colors::TITLE << "Thank you for choosing James' Café — come back soon! ☕\n\n" << Colors::RESET;
    }
};

/* -------------------- App Helpers -------------------- */
void printBackstory() {
    cout << Colors::TITLE
        << "Welcome to James' Café — A cozy corner for your calm mornings.\n"
        << Colors::RESET;
    cout << Colors::MUTED
        << "Here we brew slow, chat quietly, and make every cup with care.\n\n"
        << Colors::RESET;
}

// **NEW HELPER FUNCTION**
bool isCategorySoldOut(const vector<Item>& menu, const string& category) {
    for (const auto& item : menu) {
        if (item.category == category && item.qty > 0) {
            return false; // Found an item in stock
        }
    }
    return true; // All items in the category are out of stock
}

// **UPDATED FUNCTION**
void showCategories(const vector<Item>& menu) {
    cout << Colors::SUBTLE << "Menu categories:\n" << Colors::RESET;

    auto printCategory = [&](int num, const string& name) {
        cout << num << ") " << name;
        if (isCategorySoldOut(menu, name)) {
            cout << Colors::SOLD_OUT << " [SOLD OUT]" << Colors::RESET;
        }
        cout << "\n";
        };

    printCategory(1, "Beverages");
    printCategory(2, "Snacks");
    printCategory(3, "Meals");
    printCategory(4, "Desserts");
    cout << "0) Finish order\n";
}

vector<Item*> listAvailableInCategory(vector<Item>& menu, const string& cat) {
    vector<Item*> available;
    for (auto& it : menu) {
        if (it.category == cat && it.qty > 0) available.push_back(&it);
    }
    if (available.empty()) {
        cout << Colors::MUTED << "(No available items in " << cat << ")\n" << Colors::RESET;
        return available;
    }
    for (size_t i = 0; i < available.size(); ++i) {
        cout << (i + 1) << ") " << available[i]->name
            << "  ₱ " << fixed << setprecision(2) << available[i]->price
            << "  (" << available[i]->qty << " left)\n";
    }
    cout << "0) Back to categories\n";
    return available;
}

unsigned long long generateReceiptNumber() {
    static unsigned long long counter = 0ULL;
    unsigned long long millis = static_cast<unsigned long long>(
        chrono::duration_cast<chrono::milliseconds>(
            chrono::system_clock::now().time_since_epoch()).count());
    // combine time and counter to reduce collisions
    return (millis % 1000000000ULL) + (++counter);
}

/* -------------------- Main Program -------------------- */
int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    enableAnsiOnWindows();

    vector<Item> menu = {
        Item("Cappuccino", 140.00, 20, "Beverages"),
        Item("Latte", 150.00, 20, "Beverages"),
        Item("Iced Americano", 120.00, 20, "Beverages"),
        Item("Chocolate Milkshake", 190.00, 20, "Beverages"),
        Item("Blueberry Muffin", 75.00, 20, "Snacks"),
        Item("Garlic Parmesan Toast", 95.00, 20, "Snacks"),
        Item("Glazed Donut Holes", 100.00, 20, "Snacks"),
        Item("Chicken Wrap", 180.00, 20, "Meals"),
        Item("Garlic Rice + Burger", 220.00, 20, "Meals"),
        Item("Chicken Alfredo Pasta", 275.00, 20, "Meals"),
        Item("Chocolate Cake Slice", 130.00, 20, "Desserts"),
        Item("Fruit Parfait", 110.00, 20, "Desserts"),
        Item("Tiramisu", 270.00, 20, "Desserts")
    };

    vector<Order> allOrders;
    int customersServed = 0;

    printBackstory();

    while (true) {
        cout << Colors::ACCENT << "---- New Customer ----" << Colors::RESET << "\n";

        Order order;
        order.receiptNo = generateReceiptNumber();

        while (true) {
            cout << "Enter customer name: ";
            string name = readLineTrimmed();
            if (name.empty()) {
                cout << Colors::ERR << "Name cannot be empty.\n" << Colors::RESET;
                continue;
            }
            order.customerName = name;
            break;
        }

        bool isEatIn = readYesNo("Dine option - Eat in? or Take-Out (Y/N): ");
        order.dineOption = isEatIn ? "Eat-In" : "Take-Out";

        while (true) {
            showCategories(menu); // **UPDATED CALL**
            int catChoice = readIntInRange("Choose category (0-4): ", 0, 4);
            if (catChoice == 0) break;

            string category;
            switch (catChoice) {
            case 1: category = "Beverages"; break;
            case 2: category = "Snacks"; break;
            case 3: category = "Meals"; break;
            case 4: category = "Desserts"; break;
            default: category = ""; break;
            }
            if (category.empty()) continue;

            // **NEW LOGIC: Check if the selected category is sold out**
            if (isCategorySoldOut(menu, category)) {
                cout << Colors::ERR << "Sorry, " << category << " is completely sold out for today.\n" << Colors::RESET;
                continue; // Go back to category selection
            }

            vector<Item*> available = listAvailableInCategory(menu, category);
            // This check is technically redundant if isCategorySoldOut is used, but kept for robustness
            if (available.empty()) continue;

            int itemChoice = readIntInRange("Select item number (0 to go back): ", 0, static_cast<int>(available.size()));
            if (itemChoice == 0) continue;

            Item* chosen = available[itemChoice - 1];
            if (!chosen) { cout << Colors::ERR << "Unexpected error selecting item.\n" << Colors::RESET; continue; }

            int qty = readIntInRange("Enter quantity: ", 1, chosen->qty);

            OrderLine line{ chosen, qty };
            order.lines.push_back(line);
            chosen->qty -= qty;
            chosen->sold += qty;

            cout << Colors::HIGHL << qty << " x " << chosen->name << " added to order." << Colors::RESET << "\n";

            bool addMore = readYesNo("Add more items? (Y/N): ");
            if (!addMore) {
                bool continueOrdering = readYesNo("Continue ordering (another category)? (Y/N): ");
                if (!continueOrdering) break;
            }
        }

        if (order.lines.empty()) {
            cout << Colors::MUTED << "No items ordered. Cancelling this transaction.\n" << Colors::RESET;
        }
        else {
            order.printReceipt();
            allOrders.push_back(order);
            customersServed++;
        }

        bool next = readYesNo("Serve next customer? (Y/N): ");
        if (!next) break;
    }

    // Daily summary
    cout << Colors::TITLE << "\n=== Daily Summary ===\n" << Colors::RESET;
    double totalRevenue = 0.0;
    int totalItemsSold = 0;
    for (auto& o : allOrders) totalRevenue += o.total();
    for (auto& it : menu) totalItemsSold += it.sold;

    cout << "Customers served: " << customersServed << "\n";
    cout << "Total revenue: ₱ " << fixed << setprecision(2) << totalRevenue << "\n";
    cout << "Total items sold: " << totalItemsSold << "\n";

    Item* best = nullptr;
    for (auto& it : menu) if (!best || it.sold > best->sold) best = &it;
    if (best && best->sold > 0) cout << "Best seller: " << best->name << " (" << best->sold << " sold)\n";
    else cout << "No sales recorded.\n";

    cout << "\nRemaining inventory:\n";
    for (auto& it : menu) cout << "- " << it.name << " : " << it.qty << " left\n";

    cout << Colors::TITLE << "\nThank you for running James' Café today. Good job! ☕\n" << Colors::RESET;
    return 0;
}
