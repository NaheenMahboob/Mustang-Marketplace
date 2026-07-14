# MustangMarketplace — User Acceptance Testing (UAT)
**Group 49 | CS3307**

---

## Overview

This document records all UAT testing performed on the MustangMarketplace system.
Testing was conducted at three levels:

1. **Automated Acceptance Tests** — compiled C++ test binaries run directly against domain logic
2. **Live End-to-End API Tests** — `curl` commands exercising the running HTTP server
3. **Manual UI Test** — interactive ncurses client verification

**Total results: 161 automated acceptance tests + 54 live API tests + 12 manual UI checks = 227 checks, all passing.**

---

## Environment Setup

### Dependencies (Ubuntu / Debian)
```bash
sudo apt update
sudo apt install -y build-essential make pkg-config \
    libsqlite3-dev libssl-dev libncurses-dev libcpp-httplib-dev \
    nlohmann-json3-dev
```

### Dependencies (macOS)
```bash
brew install openssl sqlite ncurses cpp-httplib nlohmann-json
```

### Build
```bash
make all                   # builds marketplace_client, marketplace_server, and all test binaries
```

### Run the server
```bash
./marketplace_server       # listens on port 9090
```

### Run the ncurses client (separate terminal)
```bash
./marketplace_client
```

---

## Part 1 — Automated Acceptance Tests

Run all five test suites:
```bash
make test_accept_all
```

### Story 1 & 2: Register / Login
```
[PASS] 1.1 Valid @uwo.ca email -> Registration successful
[PASS] 1.2 Non-UWO email -> Invalid Domain
[PASS] 1.3 Duplicate email -> Email Already Registered
[PASS] 2.1 Correct credentials -> Login successful
[PASS] 2.2 Wrong password -> Authentication Failed
[PASS] 2.3 Unknown email -> User Not Found
[PASS] 2.4 Empty password -> Authentication Failed
[PASS] 2.5 Banned user -> Access Denied
[PASS] 2.6 First successful login establishes baseline session
[PASS] 2.7 Second successful login replaces session identity
[PASS] 2.8 Replaced session stores second user id
[PASS] 2.9 Replaced session stores second user email
```

### Story 3 & 4: Session Management
```
[PASS] 3.1 syncSessionForNetwork activates session
[PASS] 3.2 syncSessionForNetwork stores user id
[PASS] 3.3 syncSessionForNetwork stores email
[PASS] 3.4 Retained session enables legacy domain call
[PASS] 4.1 clear() closes in-memory session
[PASS] 4.2 Closed session blocks authenticated listing create
```

### Story 5: Create Listing
```
[PASS] 5.1 Valid listing -> Listing created
[PASS] 5.2 Missing description -> Invalid input
[PASS] 5.3 Negative price -> Invalid input
[PASS] 5.4 Zero price -> Listing created
[PASS] 5.5 Banned seller -> Access Denied
```

### Story 6: Delete Listing
```
[PASS] 6.1 Owner deletes listing -> Listing removed
[PASS] 6.2 Repeated delete on same listing -> Item Not Found
[PASS] 6.3 Non-owner -> Access Denied
[PASS] 6.4 Missing listing -> Item Not Found
[PASS] 6.5 Sold listing delete -> Item Not Found
[PASS] 6.6 Unauthenticated delete -> Session Expired
```

### Story 7 & 11: Search / Filtering
```
[PASS] 7.1 Keyword matches listing -> results displayed
[PASS] 7.2 Blank keyword browse -> active listings shown
[PASS] 11.1 Category filter narrows results
[PASS] 11.2 Category + keyword works together
[PASS] 11.3 Low to High price ordering works
[PASS] 11.4 High to Low price ordering works
[PASS] 7.3 Keyword with no matches -> No Results Found
[PASS] 11.5 Category with no matches -> No Results Found
```

### Story 8: Make Offer
```
[PASS] 8.1 Valid offer -> Offer submitted
[PASS] 8.2 Outgoing offers include pending offer
[PASS] 8.3 Seller receives new-offer notification
[PASS] 8.4 Invalid price -> Invalid Offer
[PASS] 8.5 Missing listing -> Item Not Found
[PASS] 8.6 Seller offers on own listing -> Access Denied
[PASS] 8.7 Sold listing -> Item Not Found
[PASS] 8.8 Deleted listing -> Item Not Found
[PASS] 8.9 Unauthenticated offer -> Session Expired
```

### Story 9: Accept / Reject Offer
```
[PASS] 9.1 Incoming offers show seller pending offer
[PASS] 9.2 Seller accepts offer -> Offer accepted
[PASS] 9.3 Accepted offer marks listing sold
[PASS] 9.4 Accepted offer records buyer on listing
[PASS] 9.5 Accepted offer keeps buyer watchlist link
[PASS] 9.6 Buyer receives accepted notification
[PASS] 9.7 Seller rejects offer -> Offer rejected
[PASS] 9.8 Non-seller tries to accept -> Access Denied
[PASS] 9.9 Accept already-closed offer -> Offer Not Found
[PASS] 9.10 Reject already-accepted offer -> Offer Not Found
[PASS] 9.11 Incoming offers exclude accepted/rejected items
[PASS] 9.12 Notifications are newest-first after reject
[PASS] 9.13 Outgoing offers include rejected status
[PASS] 9.14 Unauthenticated accept -> Session Expired
[PASS] 9.15 Unauthenticated reject -> Session Expired
```

### Story 10: Tag Listing
```
[PASS] 10.1 Valid category -> Category assigned
[PASS] 10.2 Empty category -> Selection Required
[PASS] 10.3 Invalid category -> Selection Required
[PASS] 10.4 Non-owner -> Access Denied
[PASS] 10.5 Missing item -> Item Not Found
[PASS] 10.6 Unauthenticated tag -> Session Expired
```

### Story 13: Watchlist
```
[PASS] 13.1 New item -> Added to Watchlist
[PASS] 13.2 Duplicate item -> Already on Watchlist
[PASS] 13.3 View shows saved listing
[PASS] 13.4 Remove existing item -> Removed from Watchlist
[PASS] 13.5 Remove absent item -> Removed from Watchlist
[PASS] 13.6 Unauthenticated add -> Session Expired
[PASS] 13.7 Unauthenticated remove -> Session Expired
```

### Story 14: Edit Listing
```
[PASS] 14.1 Valid edit -> Listing updated
[PASS] 14.2 Non-owner -> Access Denied
[PASS] 14.3 Empty title -> Invalid input
[PASS] 14.4 Negative price -> Invalid input
[PASS] 14.5 Missing item -> Item Not Found
[PASS] 14.6 Zero price edit -> Listing updated
[PASS] 14.7 Sold listing edit -> Item Not Found
[PASS] 14.8 Deleted listing edit -> Item Not Found
[PASS] 14.9 Unauthenticated edit -> Session Expired
```

### Story 15: Ban User (Admin)
```
[PASS] 15.1 Valid user -> banned
[PASS] 15.2 Banned user login blocked
[PASS] 15.3 Unknown user -> User Not Found
```

### Story 16: Load Listing Photo
```
[PASS] 16.1 Valid file path -> raw photo loads
[PASS] 16.2 Valid file path -> raw bytes are non-empty
[PASS] 16.3 Seeded fixture content type -> application/octet-stream
[PASS] 16.4 Broken file path -> raw load returns false
[PASS] 16.5 Broken file path clears loaded bytes
[PASS] 16.6 Broken file path clears content type
[PASS] 16.7 No photo entry -> raw load returns false
[PASS] 16.8 No photo entry clears loaded bytes
[PASS] 16.9 No photo entry clears content type
```

### Story 16B: Raw Photo Upload / Load
```
[PASS] 16B.1 Valid raw upload by owner -> Photo saved
[PASS] 16B.2 Valid raw load after upload -> returns true
[PASS] 16B.3 Loaded bytes are non-empty
[PASS] 16B.4 Loaded bytes match uploaded bytes exactly
[PASS] 16B.5 Loaded content type is image/png
[PASS] 16B.6 Re-upload replaces previous raw photo -> Photo saved
[PASS] 16B.7 Replacement raw load succeeds
[PASS] 16B.8 Replacement bytes match latest upload
[PASS] 16B.9 Replacement content type stays image/png
[PASS] 16B.10 Unauthenticated raw upload -> Session Expired
[PASS] 16B.11 Empty raw upload -> Invalid image data
[PASS] 16B.12 Non-owner raw upload -> Access Denied
[PASS] 16B.13 Missing listing raw upload -> Item Not Found
[PASS] 16B.14 Sold listing raw upload -> Item Not Available
[PASS] 16B.15 Deleted listing raw upload -> Item Not Available
[PASS] 16B.16 Raw load with no photo row -> false
[PASS] 16B.17 No photo row clears loaded bytes
[PASS] 16B.18 No photo row clears content type
[PASS] 16B.19 Raw load with broken path row -> false
[PASS] 16B.20 Broken path clears loaded bytes
[PASS] 16B.21 Broken path clears content type
```

### Story 17: Recommendations
```
[PASS] 17.0 Seed latest watchlist category with Books listing
[PASS] 17.1 Watchlisted items are excluded from matching recommendations
[PASS] 17.2 Watchlisting newest same-category item succeeds
[PASS] 17.3 Newest watched item filtered out -> next newest same-category item shown
[PASS] 17.4 Removing newest watched item succeeds
[PASS] 17.5 Older watched same-category item -> newest unwatched same-category shown
[PASS] 17.6 Watching middle same-category item succeeds
[PASS] 17.7 Latest watched middle item -> newest unwatched same-category item shown
[PASS] 17.8 Watching remaining newest same-category item succeeds
[PASS] 17.9 All same-category listings watched -> Market Empty
[PASS] 17.10 Newer watchlist item changes category
[PASS] 17.11 Only watched item in latest category -> Market Empty
[PASS] 17.12 Older same-category item can be watchlisted
[PASS] 17.13 Deleted newest watchlist item can still drive recommendations
[PASS] 17.14 Deleting non-driver listing succeeds
[PASS] 17.15 Non-driver deleted watchlist row is removed immediately
[PASS] 17.16 Deleting newest driver listing succeeds
[PASS] 17.17 Deleted newest watchlist item keeps prior category signal
[PASS] 17.18 Adding newer watchlist item cleans up deleted previous driver
[PASS] 17.19 Deleted prior latest watchlist row is unlinked after newer add
[PASS] 17.20 Recommendation now follows new latest watchlist category
[PASS] 17.21 No watchlist history -> newest active listing shown
[PASS] 17.22 No active listings -> Market Empty
[PASS] 17.23 Unauthenticated recommendation -> DB_ERROR
```

### Story 18 & 19: Ratings
```
[PASS] 18.1 Seller with no ratings reports empty state
[PASS] 18.1B Invalid seller id reports empty state
[PASS] 19.1 Precheck allows unrated purchased listing before submission
[PASS] 19.1B Invalid seller id precheck -> No completed transaction
[PASS] 19.2 Buyer can rate purchased listing
[PASS] 19.3 Same purchase cannot be rated twice
[PASS] 19.4 Precheck blocks already-rated purchase
[PASS] 19.5 Precheck allows second unrated purchase
[PASS] 19.6 Buyer can rate second purchase from same seller
[PASS] 19.7 No completed transaction -> blocked
[PASS] 19.8 Cannot rate listing bought by another buyer
[PASS] 19.9 Cannot rate active listing
[PASS] 19.10 Invalid rating value blocked
[PASS] 19.11 Missing listing cannot be rated
[PASS] 19.12 Boundary rating value 1 is accepted
[PASS] 19.13 Unauthenticated canRateSeller -> Session Expired
[PASS] 19.14 Unauthenticated rateSeller -> Session Expired
[PASS] 18.2 Seller average rating shown to one decimal place
[PASS] 18.3 Seller 1 still has no ratings
[PASS] 18.4 Banning seller succeeds
[PASS] 18.5 Banned seller returns Profile Hidden
```

**Automated test total: 161 / 161 PASS**

---

## Part 2 — Live End-to-End API Tests (curl)

Start a fresh server before running:
```bash
pkill -f marketplace_server 2>/dev/null
rm -f marketplace.db
./marketplace_server &
sleep 1
```

### Register / Login / Logout

```bash
# Register valid UWO user (alice = seller)
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"REGISTER","email":"alice@uwo.ca","password":"pw1"}'
# Expected: {"message":"Registration successful","status":"ok","token":"...","user_id":"1"}

# Register duplicate email
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"REGISTER","email":"alice@uwo.ca","password":"pw1"}'
# Expected: {"message":"Email Already Registered","status":"error"}

# Register non-UWO email
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"REGISTER","email":"alice@gmail.com","password":"pw1"}'
# Expected: {"message":"Invalid Domain","status":"error"}

# Register second user (bob = buyer)
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"REGISTER","email":"bob@uwo.ca","password":"pw2"}'
# Expected: {"message":"Registration successful","status":"ok","token":"...","user_id":"2"}

# Login correct credentials
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"LOGIN","email":"alice@uwo.ca","password":"pw1"}'
# Expected: {"message":"Login successful","status":"ok","token":"...","user_id":"1"}

# Login wrong password
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"LOGIN","email":"alice@uwo.ca","password":"wrong"}'
# Expected: {"message":"Authentication Failed","status":"error"}

# Login unknown email
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"LOGIN","email":"nobody@uwo.ca","password":"pw1"}'
# Expected: {"message":"User Not Found","status":"error"}

# Logout
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"LOGOUT","token":"<TOK_A>"}'
# Expected: {"message":"Logged out","status":"ok"}
```
```
[PASS] Register valid UWO email
[PASS] Register duplicate email
[PASS] Register non-UWO email
[PASS] Register second user
[PASS] Login correct credentials
[PASS] Login wrong password
[PASS] Login unknown email
[PASS] Logout clears session
```

---

### Create Listing

```bash
# Create valid listing (seller = alice, TOK_A = alice's session token)
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"CREATE_LISTING","token":"<TOK_A>","title":"Physics Book",
       "description":"Good condition","price":30.0,"category":"Textbook"}'
# Expected: {"listing_id":"1","message":"Listing created","status":"ok"}

# Create listing with empty description
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"CREATE_LISTING","token":"<TOK_A>","title":"Chair",
       "description":"","price":20.0,"category":"Furniture"}'
# Expected: {"message":"Invalid input","status":"error"}

# Create listing with negative price
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"CREATE_LISTING","token":"<TOK_A>","title":"X",
       "description":"Y","price":-5.0,"category":"Furniture"}'
# Expected: {"message":"Invalid input","status":"error"}
```
```
[PASS] Create listing valid
[PASS] Create listing empty description blocked
[PASS] Create listing negative price blocked
```

---

### Create Listing with Photo Upload (multipart)

```bash
curl -s -X POST http://localhost:9090/create_listing \
  -F "action=CREATE_LISTING" \
  -F "token=<TOK_A>" \
  -F "title=Camera" \
  -F "description=DSLR camera" \
  -F "price=200" \
  -F "category=Electronics" \
  -F "image=@/path/to/photo.png;type=image/png"
# Expected: {"listing_id":"3","message":"Listing created","status":"ok"}

# Retrieve photo
curl -s http://localhost:9090/photo/3 -o retrieved.png
file retrieved.png
# Expected: PNG image data, ...
```
```
[PASS] Create listing with photo upload
[PASS] Retrieve photo returns 200 (valid PNG)
```

---

### Search Listings

```bash
# Search by keyword
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"SEARCH","token":"<TOK_B>","keyword":"Physics"}'
# Expected: row_0_title = "Physics Book"

# Search by category
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"SEARCH","token":"<TOK_B>","keyword":"","category":"Furniture"}'
# Expected: results containing Furniture listings

# Search price low to high
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"SEARCH","token":"<TOK_B>","keyword":"","price_sort":"asc"}'
# Expected: status ok, prices ascending

# Search price high to low
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"SEARCH","token":"<TOK_B>","keyword":"","price_sort":"desc"}'
# Expected: status ok, prices descending

# Search no results
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"SEARCH","token":"<TOK_B>","keyword":"zzznomatch"}'
# Expected: {"count":"0","message":"OK","status":"ok"}
```
```
[PASS] Search by keyword
[PASS] Search by category
[PASS] Search price low-to-high
[PASS] Search price high-to-low
[PASS] Search no results
```

---

### My Listings

```bash
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"MY_LISTINGS","token":"<TOK_A>"}'
# Expected: alice's listings returned
```
```
[PASS] My Listings returns seller's listings
```

---

### Tag / Edit Listing

```bash
# Tag valid category
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"TAG_LISTING","token":"<TOK_A>","listing_id":1,"category":"Electronics"}'
# Expected: {"message":"Category assigned","status":"ok"}

# Tag invalid category
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"TAG_LISTING","token":"<TOK_A>","listing_id":1,"category":"BadCat"}'
# Expected: {"message":"Selection Required","status":"error"}

# Tag non-owner blocked
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"TAG_LISTING","token":"<TOK_B>","listing_id":1,"category":"Textbook"}'
# Expected: {"message":"Access Denied","status":"error"}

# Edit listing valid
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"EDIT_LISTING","token":"<TOK_A>","listing_id":1,
       "title":"Physics Book 2nd Ed","description":"Updated","price":25.0}'
# Expected: {"message":"Listing updated","status":"ok"}

# Edit non-owner blocked
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"EDIT_LISTING","token":"<TOK_B>","listing_id":1,
       "title":"Hacked","description":"Hacked","price":1.0}'
# Expected: {"message":"Access Denied","status":"error"}
```
```
[PASS] Tag listing valid category
[PASS] Tag listing invalid category blocked
[PASS] Tag listing non-owner blocked
[PASS] Edit listing valid
[PASS] Edit listing non-owner blocked
```

---

### Watchlist

```bash
# Add to watchlist
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"ADD_WATCHLIST","token":"<TOK_B>","listing_id":1}'
# Expected: {"message":"Added to Watchlist","status":"ok"}

# Add duplicate
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"ADD_WATCHLIST","token":"<TOK_B>","listing_id":1}'
# Expected: {"message":"Already on Watchlist","status":"error"}

# View watchlist
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"WATCHLIST","token":"<TOK_B>"}'
# Expected: listing 1 in results

# Remove from watchlist
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"REMOVE_WATCHLIST","token":"<TOK_B>","listing_id":1}'
# Expected: {"message":"Removed from Watchlist","status":"ok"}
```
```
[PASS] Add to watchlist
[PASS] Add duplicate to watchlist
[PASS] View watchlist
[PASS] Remove from watchlist
[PASS] Watchlist empty after remove
```

---

### Recommendations

```bash
# Add a listing to watchlist first to seed recommendation category
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"ADD_WATCHLIST","token":"<TOK_B>","listing_id":2}'

# Get recommendation
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"RECOMMEND","token":"<TOK_B>"}'
# Expected: a listing matching the watchlisted category
```
```
[PASS] Recommendation returns a listing
```

---

### Offers

```bash
# Make valid offer (bob offers $20 on listing 1)
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"MAKE_OFFER","token":"<TOK_B>","listing_id":1,"offer_price":20.0}'
# Expected: {"message":"Offer submitted","status":"ok"}

# Seller cannot offer on own listing
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"MAKE_OFFER","token":"<TOK_A>","listing_id":1,"offer_price":20.0}'
# Expected: {"message":"Access Denied","status":"error"}

# Negative offer price blocked
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"MAKE_OFFER","token":"<TOK_B>","listing_id":1,"offer_price":-1.0}'
# Expected: {"message":"Invalid Offer","status":"error"}

# Buyer views their outgoing offers
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"MY_OFFERS","token":"<TOK_B>"}'
# Expected: offer with status "pending"

# Seller views incoming offers
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"MY_INCOMING_OFFERS","token":"<TOK_A>"}'
# Expected: offer from bob with status Pending

# Seller rejects offer
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"REJECT_OFFER","token":"<TOK_A>","offer_id":1}'
# Expected: {"message":"Offer rejected","status":"ok"}

# Make new offer and accept
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"MAKE_OFFER","token":"<TOK_B>","listing_id":1,"offer_price":22.0}'

curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"ACCEPT_OFFER","token":"<TOK_A>","offer_id":2}'
# Expected: {"message":"Offer accepted","status":"ok"}
```
```
[PASS] Make offer valid
[PASS] Seller cannot offer on own listing
[PASS] Offer negative price blocked
[PASS] Buyer my offers shows status
[PASS] Seller incoming offers
[PASS] Seller rejects offer
[PASS] Make second offer
[PASS] Seller accepts offer
```

---

### Notifications

```bash
# Seller notifications (offer received)
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"NOTIFICATIONS","token":"<TOK_A>"}'
# Expected: note containing "Offer: $20.00 for Physics Book from bob"

# Buyer notifications (offer accepted)
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"NOTIFICATIONS","token":"<TOK_B>"}'
# Expected: note containing "accepted"
```
```
[PASS] Seller has offer notifications
[PASS] Buyer has accepted notification
```

---

### My Purchases / Ratings

```bash
# Buyer views purchases
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"MY_PURCHASES","token":"<TOK_B>"}'
# Expected: "Physics Book" with status "sold"

# View seller rating before any ratings
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"VIEW_RATING","token":"<TOK_B>","seller_id":1}'
# Expected: status ok, no ratings yet

# Check rating eligibility
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"CAN_RATE_SELLER","token":"<TOK_B>","listing_id":1,"seller_id":1}'
# Expected: {"message":"OK","status":"ok"}

# Submit rating
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"RATE_SELLER","token":"<TOK_B>","listing_id":1,
       "seller_id":1,"rating":5,"comment":"Great seller!"}'
# Expected: {"message":"Rating submitted","status":"ok"}

# Submit rating again (blocked)
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"RATE_SELLER","token":"<TOK_B>","listing_id":1,
       "seller_id":1,"rating":4,"comment":"Again"}'
# Expected: {"status":"error"}

# View rating after submission
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"VIEW_RATING","token":"<TOK_B>","seller_id":1}'
# Expected: average_rating contains "5"
```
```
[PASS] Buyer sees purchased listing
[PASS] View seller rating (no ratings yet)
[PASS] Can rate seller eligible
[PASS] Rate seller submit
[PASS] Rate seller twice blocked
[PASS] View seller rating after submission
```

---

### Delete Listing

```bash
# Non-owner delete blocked
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"DELETE_LISTING","token":"<TOK_B>","listing_id":2}'
# Expected: {"message":"Access Denied","status":"error"}

# Owner deletes listing
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"DELETE_LISTING","token":"<TOK_A>","listing_id":2}'
# Expected: {"message":"Listing removed","status":"ok"}

# Delete already-deleted listing
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"DELETE_LISTING","token":"<TOK_A>","listing_id":2}'
# Expected: {"message":"Item Not Found","status":"error"}
```
```
[PASS] Non-owner delete blocked
[PASS] Owner deletes listing
[PASS] Delete already-deleted listing
```

---

### Admin — Ban User

```bash
# Wrong admin code
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"BAN_USER","token":"<TOK_A>","admin_code":"wrongcode","user_id":2}'
# Expected: {"message":"Access Denied: invalid admin code.","status":"error"}

# Correct admin code (code is: admin123)
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"BAN_USER","token":"<TOK_A>","admin_code":"admin123","user_id":2}'
# Expected: {"message":"User banned","status":"ok"}

# Banned user cannot login
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"LOGIN","email":"bob@uwo.ca","password":"pw2"}'
# Expected: {"message":"Access Denied","status":"error"}
```
```
[PASS] Ban with wrong code blocked
[PASS] Ban user with valid code
[PASS] Banned user cannot login
```

---

### Auth Edge Cases

```bash
# Invalid/expired token
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"SEARCH","token":"invalidtoken","keyword":""}'
# Expected: {"message":"Session Expired","status":"error"}

# Unknown action
curl -s -X POST http://localhost:9090/api \
  -H "Content-Type: application/json" \
  -d '{"action":"UNKNOWN_ACTION","token":"<TOK_A>"}'
# Expected: {"message":"Unknown action","status":"error"}
```
```
[PASS] Invalid token rejected
[PASS] Unknown action returns error
```

---

## Part 3 — Manual UI Test (ncurses client)

Performed interactively using `./marketplace_client`:

| Step | Action | Result |
|------|--------|--------|
| 1 | Launch client | Landing screen shown |
| 2 | Register `seller@uwo.ca` / `pass123` | "Registration successful" |
| 3 | Create listing "Calculus Textbook" $45 with photo upload (`test.png`) | "Listing created" |
| 4 | Register `buyer@uwo.ca` / `pass456` in separate session | "Registration successful" |
| 5 | Search "Calculus" | Listing found: $45.00, Textbook, seller@uwo.ca |
| 6 | Make offer $40 | "Offer submitted" |
| 7 | Login as seller, view Notifications | "Offer: $40.00 for Calculus Textbook from buyer" |
| 8 | View Incoming Offers | Offer from buyer@uwo.ca — Pending |
| 9 | Accept offer | "Offer accepted" |
| 10 | Login as buyer, view Notifications | "...accepted" notification |
| 11 | My Purchases → View Photo | Photo opens in OS image viewer (374×358 PNG) |
| 12 | My Purchases → Rate Seller (5 stars) | "Rating submitted" |

---

## Summary

| Test Level | Tests Run | Passed | Failed |
|---|---|---|---|
| Automated acceptance tests | 161 | 161 | 0 |
| Live curl API tests | 54 | 54 | 0 |
| Manual ncurses UI test | 12 | 12 | 0 |
| **Total** | **227** | **227** | **0** |

All user stories covered: **1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 14, 15, 16, 17, 18, 19**
