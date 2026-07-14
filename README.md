# Mustang Marketplace README

## About This Project

Mustang Marketplace was developed as a group project for **CS3307 - Objected-Oriented Design and Analysis** —
a terminal-based (ncurses) online marketplace application with a C++ client,
an HTTP API server, and a SQLite-backed data layer. Development was originally
hosted on Western's GitLab; this repo was recreated from local project files
after student GitLab access was closed.

### My Contributions

**Client UI**
- Built the full ncurses client interface (`main.cpp`) — all screens, forms, input handling, and navigation flows
- Photo-specific UI flows: caching and opening raw image bytes in the OS viewer, photo selection during listing edit, and photo display within listing detail views

**API Client**
- `ApiListing` data structure (parsed listing row returned to the client)
- `ApiOffer` data structure (parsed offer row returned to the client)
- Photo retrieval over the API (`viewPhotoRaw`)
- Local image file reading for uploads (`readUploadFile`)
- Multipart listing create/edit request handling, including image attachment
- JSON request/response handling for the client (`sendJsonRequest`)
- Response parsing into listings and offers (`parseListings`, `parseOffers`)

**Server Request Handling** (`server_main.cpp`)
- Schema bootstrap on server startup
- `MultipartActionRequest` representation and multipart form parsing/file-upload handling
- Session token generation and authentication (`makeToken`, `authenticateToken`)
- JSON field-reading helpers (string/int/double)
- Login, registration, and logout request handlers
- `handleAction` logic for: watchlist (add/remove/view), listing create/edit/tag/delete, listing search, and "my listings" retrieval

**Listings**
- Search/filter logic for active listings (`search`)
- Seller's own listings view (`view`)
- Buyer purchase history view (`viewPurchases`)

**Photos** (`Photo.cpp`/`.h`)
- Photo entity (id, listing id, file path)
- Raw image load/save (`loadRaw`, `saveRaw`)
- Legacy photo view flow (`view`)
- Add/replace/remove photo by path (`add`)
- Asset root configuration and path resolution (`setAssetRoot`, `resolveStoredPath`)
- Supporting helpers: image-type detection (`pickExtension`), listing-edit authorization checks (`verifyEditableListing`), safe upload-file cleanup (`unlinkIfUploadPath`)

**User/Session**
- Session snapshot logic and registration flow updates to shared session state

**Watchlist** (fully authored)
- Add, remove, and view watchlist entries

**Shared Utilities** (fully authored)
- TSV serialization helpers (`TsvUtil`)
- Protocol-level constants and limits (`ProtocolLimits.h`)
- Response-building layer converting domain data into JSON API responses (`ResponseBuilder`)
- Username/category validation rules (`MarketplaceRules.h`)

**Testing & Documentation**
- Wrote acceptance tests for the areas above, along with additional coverage across other parts of the codebase
- Collaborated on `UAT_TESTING.md`, documenting 227 passing checks across automated, live API, and manual test levels

---

## Project Structure

This project is split into:

- `main.cpp` + `ApiClient.*`: presentation tier (ncurses client)
- `server_main.cpp` + domain entities: application tier (network API server)
- `Database.sql` + SQLite access in server: data tier

## Prerequisites

This project is built with:

- a C++17 compiler
- `make`
- SQLite3 development libraries
- OpenSSL / libcrypto development libraries
- ncurses development libraries
- `cpp-httplib`
- `nlohmann/json`

### Ubuntu / Debian

Refresh package metadata and install the required build dependencies:

```bash
sudo apt update
sudo apt install -y build-essential make pkg-config \
    libsqlite3-dev libssl-dev libncurses-dev libcpp-httplib-dev \
    nlohmann-json3-dev
```

If your system already has the packages but you want the latest available
versions from your configured repositories, run:

```bash
sudo apt update
sudo apt upgrade
```

### macOS

Install the Xcode Command Line Tools first:

```bash
xcode-select --install
```

Then install the project dependencies with Homebrew:

```bash
brew update
brew install openssl sqlite ncurses cpp-httplib nlohmann-json
```

If you want to upgrade already-installed Homebrew packages first:

```bash
brew update
brew upgrade
```

On some macOS systems, Homebrew libraries are installed outside the default
compiler search path. If `make all` fails with missing headers or linker errors,
build with explicit Homebrew include and library paths:

Intel Mac:

```bash
make all \
  CXX=clang++ \
  CXXFLAGS="-std=c++17 -Wall -Wextra -g -I. -I/usr/local/include" \
  LIBS_CLIENT="-L/usr/local/lib -lncurses -lcpp-httplib" \
  LIBS_SERVER="-L/usr/local/lib -lsqlite3 -lcrypto -lcpp-httplib" \
  LIBS_TEST="-L/usr/local/lib -lsqlite3 -lcrypto"
```

Apple Silicon Mac:

```bash
make all \
  CXX=clang++ \
  CXXFLAGS="-std=c++17 -Wall -Wextra -g -I. -I/opt/homebrew/include" \
  LIBS_CLIENT="-L/opt/homebrew/lib -lncurses -lcpp-httplib" \
  LIBS_SERVER="-L/opt/homebrew/lib -lsqlite3 -lcrypto -lcpp-httplib" \
  LIBS_TEST="-L/opt/homebrew/lib -lsqlite3 -lcrypto"
```

## Build

```bash
make all
```

## Run

Terminal 1:

```bash
./marketplace_server
```

Terminal 2:

```bash
./marketplace_client
```

The client connects to `127.0.0.1:9090` over plain TCP (one request line per connection).

## Acceptance Tests

The project includes grouped acceptance-test binaries for the user stories.
To build and run all of them:

```bash
make test_accept_all
```

## Optional: Connect to a remote server machine

You can point the client at another machine on your network using environment variables:

```bash
MARKETPLACE_HOST=<server-lan-ip> MARKETPLACE_PORT=9090 ./marketplace_client
```

If unset, defaults are:

- `MARKETPLACE_HOST=127.0.0.1`
- `MARKETPLACE_PORT=9090`
