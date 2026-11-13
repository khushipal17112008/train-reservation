/*
 * Train Ticket Reservation System
 * Language : C
 * Features : train list, search, booking, seat allocation, PNR status,
 *            cancellation with refund, file storage
 *
 * Note     : train names, numbers, timings and fares are SAMPLE data.
 *
 * Compile  : gcc train_reservation.c -o train
 * Run      : ./train          (on Windows: train.exe)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define TRAINS          5
#define CLASSES         3
#define MAX_PASS        4        /* passengers in one booking */
#define SEATS_PER_CLASS 40
#define MAX_BOOKINGS    300
#define DATA_FILE       "reservations.dat"

/* ---------- Data structures ---------- */

struct Train {
    int  no;
    char name[30];
    char from[20];
    char to[20];
    char dep[8];
    char arr[8];
    int  fare[CLASSES];          /* fare for SL, 3A, 2A */
};

struct Passenger {
    char name[30];
    int  age;
    char gender;
    int  seat;
};

struct Booking {
    int  pnr;
    char phone[11];
    int  train;                  /* index in trains[] */
    int  cls;                    /* index in className[] */
    int  d, m, y;                /* journey date */
    int  count;
    struct Passenger p[MAX_PASS];
    int  fare;
    int  active;                 /* 1 = confirmed, 0 = cancelled */
};

const struct Train trains[TRAINS] = {
    { 12951, "Rajdhani Express",  "Delhi", "Mumbai",    "16:30", "08:15", { 1200, 1850, 2600 } },
    { 12009, "Shatabdi Express",  "Delhi", "Jaipur",    "06:05", "10:40", {  300,  550,  800 } },
    { 12301, "Howrah Mail",       "Delhi", "Kolkata",   "17:00", "10:05", {  900, 1500, 2100 } },
    { 12627, "Karnataka Express", "Delhi", "Bengaluru", "21:15", "06:30", { 1100, 1750, 2400 } },
    { 12723, "Telangana Express", "Delhi", "Hyderabad", "06:50", "05:30", { 1000, 1650, 2300 } }
};

const char *className[CLASSES]  = { "Sleeper (SL)", "AC 3 Tier (3A)", "AC 2 Tier (2A)" };
const char  classLetter[CLASSES] = { 'S', 'B', 'A' };

struct Booking bookings[MAX_BOOKINGS];
int totalBookings = 0;
int nextPnr = 200001;

/* ---------- Input helpers ---------- */

void readLine(char *buf, int size) {
    if (fgets(buf, size, stdin) == NULL) {
        buf[0] = '\0';
        return;
    }
    buf[strcspn(buf, "\n")] = '\0';
}

int readInt(const char *prompt, int min, int max) {
    char line[50];
    int value;
    while (1) {
        printf("%s", prompt);
        readLine(line, sizeof(line));
        if (sscanf(line, "%d", &value) == 1 && value >= min && value <= max)
            return value;
        printf("  Invalid input. Enter a number between %d and %d.\n", min, max);
    }
}

int validPhone(const char *p) {
    int i;
    if (strlen(p) != 10) return 0;
    if (p[0] < '6' || p[0] > '9') return 0;
    for (i = 0; i < 10; i++)
        if (!isdigit((unsigned char)p[i])) return 0;
    return 1;
}

int validName(const char *s) {
    int i, len = (int)strlen(s);
    if (len < 2) return 0;
    for (i = 0; i < len; i++)
        if (!isalpha((unsigned char)s[i]) && s[i] != ' ') return 0;
    return 1;
}

/* ---------- Date helpers ---------- */

int isLeap(int y) {
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

int daysInMonth(int m, int y) {
    int days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (m == 2 && isLeap(y)) return 29;
    return days[m - 1];
}

void getToday(int *d, int *m, int *y) {
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    *d = lt->tm_mday;
    *m = lt->tm_mon + 1;
    *y = lt->tm_year + 1900;
}

/* compare two dates: returns <0, 0, >0 */
int compareDate(int d1, int m1, int y1, int d2, int m2, int y2) {
    if (y1 != y2) return y1 - y2;
    if (m1 != m2) return m1 - m2;
    return d1 - d2;
}

void readJourneyDate(int *d, int *m, int *y) {
    int td, tm_, ty;
    getToday(&td, &tm_, &ty);
    while (1) {
        printf("\nEnter journey date (today is %02d-%02d-%04d)\n", td, tm_, ty);
        *d = readInt("  Day   : ", 1, 31);
        *m = readInt("  Month : ", 1, 12);
        *y = readInt("  Year  : ", ty, ty + 1);
        if (*d > daysInMonth(*m, *y)) {
            printf("  That date does not exist. Try again.\n");
            continue;
        }
        if (compareDate(*d, *m, *y, td, tm_, ty) < 0) {
            printf("  Date is in the past. Try again.\n");
            continue;
        }
        return;
    }
}

/* ---------- File handling ---------- */

void saveData(void) {
    FILE *fp = fopen(DATA_FILE, "wb");
    if (fp == NULL) {
        printf("Warning: could not save data.\n");
        return;
    }
    fwrite(&nextPnr, sizeof(int), 1, fp);
    fwrite(&totalBookings, sizeof(int), 1, fp);
    fwrite(bookings, sizeof(struct Booking), totalBookings, fp);
    fclose(fp);
}

void loadData(void) {
    FILE *fp = fopen(DATA_FILE, "rb");
    if (fp == NULL) return;                      /* first run: no file yet */
    if (fread(&nextPnr, sizeof(int), 1, fp) != 1 ||
        fread(&totalBookings, sizeof(int), 1, fp) != 1 ||
        totalBookings < 0 || totalBookings > MAX_BOOKINGS ||
        fread(bookings, sizeof(struct Booking), totalBookings, fp) != (size_t)totalBookings) {
        totalBookings = 0;
        nextPnr = 200001;
    }
    fclose(fp);
}

/* ---------- Seat management ---------- */

/* marks used seats (1..40) for a train, class and date; returns count */
int getUsedSeats(int t, int c, int d, int m, int y, int used[]) {
    int i, j, count = 0;
    for (i = 0; i <= SEATS_PER_CLASS; i++) used[i] = 0;
    for (i = 0; i < totalBookings; i++) {
        struct Booking *b = &bookings[i];
        if (!b->active || b->train != t || b->cls != c) continue;
        if (b->d != d || b->m != m || b->y != y) continue;
        for (j = 0; j < b->count; j++) {
            used[b->p[j].seat] = 1;
            count++;
        }
    }
    return count;
}

void seatLabel(int c, int seat, char *out) {
    sprintf(out, "%c%d/%d", classLetter[c], (seat - 1) / 8 + 1, (seat - 1) % 8 + 1);
}

/* ---------- Fare ---------- */

int passengerFare(int baseFare, int age) {
    if (age < 5) return 0;                       /* child under 5 travels free */
    if (age >= 60) return baseFare * 60 / 100;   /* 40% senior citizen concession */
    return baseFare;
}

/* ---------- Display functions ---------- */

void showTrains(void) {
    int i;
    printf("\n%-3s %-6s %-19s %-10s %-10s %-6s %-6s\n", "No", "Train", "Name", "From", "To", "Dep", "Arr");
    printf("-----------------------------------------------------------------------\n");
    for (i = 0; i < TRAINS; i++)
        printf("%-3d %-6d %-19s %-10s %-10s %-6s %-6s\n", i + 1, trains[i].no,
               trains[i].name, trains[i].from, trains[i].to, trains[i].dep, trains[i].arr);
}

void printTicket(const struct Booking *b) {
    int i;
    char label[16];
    printf("\n+------------------------------------------------+\n");
    printf("|              RAILWAY E-TICKET                  |\n");
    printf("+------------------------------------------------+\n");
    printf("  PNR        : %d\n", b->pnr);
    printf("  Train      : %d %s\n", trains[b->train].no, trains[b->train].name);
    printf("  Route      : %s to %s\n", trains[b->train].from, trains[b->train].to);
    printf("  Date       : %02d-%02d-%04d    Dep: %s\n", b->d, b->m, b->y, trains[b->train].dep);
    printf("  Class      : %s\n", className[b->cls]);
    printf("  Contact    : %s\n", b->phone);
    printf("  Passengers :\n");
    for (i = 0; i < b->count; i++) {
        seatLabel(b->cls, b->p[i].seat, label);
        printf("    %d. %-20s %3d  %c   Coach/Berth: %s\n", i + 1, b->p[i].name,
               b->p[i].age, b->p[i].gender, label);
    }
    printf("  Total fare : Rs.%d\n", b->fare);
    printf("  Status     : %s\n", b->active ? "CONFIRMED" : "CANCELLED");
    printf("+------------------------------------------------+\n");
}

/* ---------- Main features ---------- */

void searchTrains(void) {
    char from[20], to[20], f1[20], t1[20], a[20], b[20];
    int i, k, found = 0;

    printf("Enter source city (press Enter for any): ");
    readLine(from, sizeof(from));
    printf("Enter destination city (press Enter for any): ");
    readLine(to, sizeof(to));

    for (k = 0; from[k]; k++) f1[k] = (char)tolower((unsigned char)from[k]);
    f1[k] = '\0';
    for (k = 0; to[k]; k++) t1[k] = (char)tolower((unsigned char)to[k]);
    t1[k] = '\0';

    printf("\nMatching trains:\n");
    for (i = 0; i < TRAINS; i++) {
        for (k = 0; trains[i].from[k]; k++) a[k] = (char)tolower((unsigned char)trains[i].from[k]);
        a[k] = '\0';
        for (k = 0; trains[i].to[k]; k++) b[k] = (char)tolower((unsigned char)trains[i].to[k]);
        b[k] = '\0';

        if (strstr(a, f1) && strstr(b, t1)) {
            printf("  %d. %d %s  (%s to %s, dep %s)\n", i + 1, trains[i].no,
                   trains[i].name, trains[i].from, trains[i].to, trains[i].dep);
            found = 1;
        }
    }
    if (!found) printf("  No trains found.\n");
}

void bookTicket(void) {
    struct Booking b;
    int used[SEATS_PER_CLASS + 1];
    int t, c, i, n, avail, usedCount, nextSeat;
    char line[50];

    if (totalBookings >= MAX_BOOKINGS) {
        printf("\nBooking limit reached.\n");
        return;
    }
    memset(&b, 0, sizeof(b));

    showTrains();
    t = readInt("\nSelect train (1-5): ", 1, TRAINS) - 1;

    printf("\nClasses and fares:\n");
    for (i = 0; i < CLASSES; i++)
        printf("  %d. %-16s Rs.%d\n", i + 1, className[i], trains[t].fare[i]);
    c = readInt("Select class: ", 1, CLASSES) - 1;

    readJourneyDate(&b.d, &b.m, &b.y);

    usedCount = getUsedSeats(t, c, b.d, b.m, b.y, used);
    avail = SEATS_PER_CLASS - usedCount;
    printf("\nSeats available: %d of %d\n", avail, SEATS_PER_CLASS);
    if (avail == 0) {
        printf("Sorry, this class is full on the selected date.\n");
        return;
    }

    n = readInt("Number of passengers (1-4): ", 1, MAX_PASS);
    if (n > avail) {
        printf("Only %d seat(s) left. Booking cancelled.\n", avail);
        return;
    }

    nextSeat = 1;
    for (i = 0; i < n; i++) {
        printf("\nPassenger %d\n", i + 1);

        printf("  Name   : ");
        readLine(b.p[i].name, sizeof(b.p[i].name));
        while (!validName(b.p[i].name)) {
            printf("  Use letters only (min 2). Name   : ");
            readLine(b.p[i].name, sizeof(b.p[i].name));
        }

        b.p[i].age = readInt("  Age    : ", 1, 120);

        printf("  Gender (M/F/O): ");
        readLine(line, sizeof(line));
        while (!(toupper((unsigned char)line[0]) == 'M' || toupper((unsigned char)line[0]) == 'F' ||
                 toupper((unsigned char)line[0]) == 'O') || line[1] != '\0') {
            printf("  Enter M, F or O: ");
            readLine(line, sizeof(line));
        }
        b.p[i].gender = (char)toupper((unsigned char)line[0]);

        /* allocate the lowest free seat number */
        while (used[nextSeat]) nextSeat++;
        used[nextSeat] = 1;
        b.p[i].seat = nextSeat;

        b.fare += passengerFare(trains[t].fare[c], b.p[i].age);
    }

    printf("\nContact mobile number: ");
    readLine(b.phone, sizeof(b.phone));
    while (!validPhone(b.phone)) {
        printf("  Enter a valid 10-digit mobile number: ");
        readLine(b.phone, sizeof(b.phone));
    }

    b.pnr = nextPnr;
    b.train = t;
    b.cls = c;
    b.count = n;
    b.active = 1;

    printf("\nTotal fare: Rs.%d\n", b.fare);
    printf("(Children under 5 free, senior citizens 60+ get 40%% off)\n");
    if (readInt("Confirm booking? (1 = Yes, 0 = No): ", 0, 1) == 0) {
        printf("Booking discarded.\n");
        return;
    }

    bookings[totalBookings++] = b;
    nextPnr++;
    saveData();
    printf("\nBooking successful! Please note your PNR: %d\n", b.pnr);
    printTicket(&b);
}

int findBooking(int pnr) {
    int i;
    for (i = 0; i < totalBookings; i++)
        if (bookings[i].pnr == pnr) return i;
    return -1;
}

void checkPnr(void) {
    int pnr = readInt("Enter PNR number: ", 1, 999999);
    int idx = findBooking(pnr);
    if (idx == -1) printf("No booking found for PNR %d.\n", pnr);
    else printTicket(&bookings[idx]);
}

void cancelTicket(void) {
    int pnr = readInt("Enter PNR to cancel: ", 1, 999999);
    int idx = findBooking(pnr);
    int refund;

    if (idx == -1) {
        printf("No booking found for PNR %d.\n", pnr);
        return;
    }
    if (!bookings[idx].active) {
        printf("This ticket is already cancelled.\n");
        return;
    }

    printTicket(&bookings[idx]);
    refund = bookings[idx].fare * 80 / 100;
    printf("Cancellation charge is 20%%. Refund amount: Rs.%d\n", refund);

    if (readInt("Cancel this ticket? (1 = Yes, 0 = No): ", 0, 1) == 1) {
        bookings[idx].active = 0;
        saveData();
        printf("Ticket cancelled. Rs.%d will be refunded.\n", refund);
    }
}

void viewAllBookings(void) {
    int i, found = 0;
    printf("\n%-8s %-6s %-19s %-11s %-6s %s\n", "PNR", "Train", "Name", "Date", "Pax", "Fare");
    printf("---------------------------------------------------------------\n");
    for (i = 0; i < totalBookings; i++) {
        if (!bookings[i].active) continue;
        found = 1;
        printf("%-8d %-6d %-19s %02d-%02d-%04d  %-6d Rs.%d\n", bookings[i].pnr,
               trains[bookings[i].train].no, trains[bookings[i].train].name,
               bookings[i].d, bookings[i].m, bookings[i].y,
               bookings[i].count, bookings[i].fare);
    }
    if (!found) printf("No active bookings.\n");
}

/* ---------- Main ---------- */

int main(void) {
    int choice;

    loadData();

    do {
        printf("\n=========================================\n");
        printf("   TRAIN TICKET RESERVATION SYSTEM\n");
        printf("=========================================\n");
        printf("  1. View all trains\n");
        printf("  2. Search trains\n");
        printf("  3. Book ticket\n");
        printf("  4. Check PNR status\n");
        printf("  5. Cancel ticket\n");
        printf("  6. View all bookings\n");
        printf("  7. Exit\n");
        choice = readInt("Enter your choice: ", 1, 7);

        switch (choice) {
            case 1: showTrains();      break;
            case 2: searchTrains();    break;
            case 3: bookTicket();      break;
            case 4: checkPnr();        break;
            case 5: cancelTicket();    break;
            case 6: viewAllBookings(); break;
            case 7: printf("\nThank you for using the reservation system!\n"); break;
        }
    } while (choice != 7);

    return 0;
}
