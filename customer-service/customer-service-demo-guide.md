# Customer Service — Project Overview & Demo Guide

## What This Project Is

A production-style microservice built in C++17, deployed to AWS using free-tier resources. It exposes a REST API for managing customer records backed by a PostgreSQL database on Amazon RDS, containerized with Docker on an EC2 instance. A standalone C++ CLI client drives the API from the command line.

This project demonstrates the full lifecycle of a cloud-deployed service: design, implement, containerize, and deploy — using the same tools and patterns used in production AWS environments.

---

## System Architecture

```
┌─────────────────────────────────┐
│        C++ CLI Client           │
│     (runs on laptop / WSL2)     │
│       customer_cli binary       │
└────────────────┬────────────────┘
                 │
         HTTP REST / JSON
           (port 8080)
                 │
┌────────────────▼────────────────┐
│         AWS EC2 t3.micro        │
│   ┌─────────────────────────┐   │
│   │   Docker Container      │   │
│   │   Crow HTTP Service     │   │
│   │   C++17 · libpqxx       │   │
│   │   nlohmann/json         │   │
│   └─────────────────────────┘   │
└────────────────┬────────────────┘
                 │
        libpqxx · SSL/TLS
           (port 5432)
                 │
┌────────────────▼────────────────┐
│      AWS RDS PostgreSQL 15      │
│         db.t4g.micro             │
│   customerdb · 20 GB gp2        │
│   Not publicly accessible       │
└─────────────────────────────────┘

  EC2 and RDS share the same Default VPC.
  RDS is only reachable from the EC2 security group.
```

---

## REST API Endpoints

| Method | Endpoint | Body | Description |
|--------|----------|------|-------------|
| GET | `/health` | — | Health check |
| POST | `/customers` | `{name, email}` | Create a customer |
| GET | `/customers` | — | List all customers |
| GET | `/customers/:id` | — | Get one customer |
| PUT | `/customers/:id` | `{name?} {email?}` | Update name or email |
| DELETE | `/customers/:id` | — | Delete a customer |

---

## Data Model

```sql
CREATE TABLE customers (
    id         SERIAL       PRIMARY KEY,
    name       VARCHAR(255) NOT NULL,
    email      VARCHAR(255) NOT NULL UNIQUE,
    created_at TIMESTAMPTZ  NOT NULL DEFAULT NOW()
);
```

---

## Technology Stack

| Layer | Technology | Why |
|-------|-----------|-----|
| Language | C++17 | Systems-level performance and expertise |
| HTTP framework | Crow v1.1 (header-only) | Lightweight, minimal dependencies |
| DB client | libpqxx 6.4 | Standard C++ PostgreSQL client |
| JSON | nlohmann/json | Header-only, clean API |
| Database | PostgreSQL 15 on RDS | Managed, ACID-compliant, automatic backups |
| Container | Docker (multi-stage build) | Reproducible builds, minimal runtime image |
| Compute | EC2 t2.micro | Free tier, demonstrates cloud deployment |
| Networking | VPC Security Groups | Least-privilege access between tiers |
| Local dev | Docker Compose | Mirrors production topology locally |
| Client | C++ CLI (cpp-httplib) | Self-contained binary, no runtime deps |

---

## Security Design

**Network isolation**
- RDS has *Publicly accessible: No* — unreachable from the internet
- The RDS security group allows port 5432 only from the EC2 security group, not from any IP range
- EC2 security group allows port 8080 only from your specific IP, not `0.0.0.0/0`
- SSH (port 22) is similarly locked to your IP

**Transport security**
- All DB connections use `sslmode=require` in production — traffic is encrypted end-to-end
- `sslmode=disable` is used only in local `docker-compose` against the local dev Postgres container

**SQL injection prevention**
- Every query uses libpqxx `exec_params()` with parameterized placeholders — user input never touches the query string directly

**Least privilege**
- Database credentials are injected at runtime via environment variables, never baked into the Docker image
- The container runs as a non-root user (uid 1001)

---

## AWS Console Demo — What to Show

### Screen 1 — EC2 Instances

**Navigate to:** EC2 → Instances

**What to show:**
- Instance name: `customer-service-backend`
- Instance state: **Running**
- Instance type: **t2.micro** (free tier)
- Public IPv4 address (this is what the CLI points at)

**What to say:** *"This is the compute layer. The Crow HTTP service runs as a Docker container on this instance. The public IP is what the CLI uses to make API calls."*

---

### Screen 2 — EC2 Security Group

**Navigate to:** EC2 → Security Groups → `customer-service-sg`

**What to show:**
- Inbound rule: port 22 (SSH) — source: My IP
- Inbound rule: port 8080 (API) — source: My IP
- Outbound: all traffic allowed (for RDS and package downloads)

**What to say:** *"Port 8080 is only open to my IP, not the public internet. In a production setup this would be behind a load balancer, but for a free-tier demo this keeps it secure while still accessible."*

---

### Screen 3 — RDS Database

**Navigate to:** RDS → Databases → `customer-db`

**What to show:**
- Status: **Available**
- Engine: **PostgreSQL 15**
- Instance class: **db.t3.micro** (free tier)
- Endpoint: the hostname the backend connects to
- Publicly accessible: **No**

**What to say:** *"The database is not publicly accessible — it can only be reached from within the VPC. The EC2 instance connects to it privately over SSL using the endpoint shown here."*

---

### Screen 4 — RDS Connectivity & Security

**Navigate to:** RDS → Databases → `customer-db` → Connectivity & security tab

**What to show:**
- VPC: Default VPC
- Subnets: the subnets the DB is in
- VPC security groups: `customer-service-rds-sg`
- Publicly accessible: **No**

**Click the RDS security group** and show:
- Inbound rule: port 5432 — source: `customer-service-sg` (the EC2 security group, not an IP)

**What to say:** *"The RDS security group only allows connections from the EC2 security group on port 5432. This means even if someone got into the VPC, they'd need to be coming from the EC2 instance to reach the database."*

---

### Screen 5 — RDS Monitoring (live during demo)

**Navigate to:** RDS → Databases → `customer-db` → Monitoring tab

**What to show while running CLI commands:**
- `DatabaseConnections` — spikes when `customer_cli list` runs
- `ReadIOPS` / `WriteIOPS` — activity visible during create/update/delete
- `FreeStorageSpace` — shows remaining free space on the 20 GB volume

**What to say:** *"You can see the connection count go up in real time when the CLI hits the API. CloudWatch gives you this observability out of the box with RDS — no extra instrumentation needed."*

---

### Screen 6 — EC2 Monitoring

**Navigate to:** EC2 → Instances → select instance → Monitoring tab

**What to show:**
- CPU Utilization — low baseline, small spike during API calls
- Network In / Network Out — traffic visible when CLI is active

**What to say:** *"Same observability on the compute side. For a production service you'd set CloudWatch alarms on CPU and network thresholds to trigger auto-scaling or alerts."*

---

### Screen 7 — Billing (close with this)

**Navigate to:** AWS Console → Billing → Free Tier

**What to show:**
- EC2: t2.micro hours used vs 750 free hours/month
- RDS: t3.micro hours used vs 750 free hours/month
- Total charges: **$0.00**

**What to say:** *"The entire stack — EC2, RDS, storage, data transfer — runs within the free tier. I also have a billing alert set at $5 so I'd get an email before anything meaningful was charged."*

---

## Live Demo Script

Run these commands from your WSL2 terminal while the interviewer watches:

```bash
# Point the CLI at your EC2 instance
export API_URL=http://<your-ec2-public-ip>:8080

# 1. Health check — confirms service is up
./customer_cli list

# 2. Create two customers
./customer_cli create "Alice Smith" alice@example.com
./customer_cli create "Bob Jones" bob@example.com

# 3. List — show both records with auto-assigned IDs and timestamps
./customer_cli list

# 4. Fetch one by ID
./customer_cli get 1

# 5. Partial update — only name changes, email and timestamp preserved
./customer_cli update 1 --name "Alice Cooper"

# 6. Try to create a duplicate email — show 409 conflict handling
./customer_cli create "Alice Duplicate" alice@example.com

# 7. Delete
./customer_cli delete 2

# 8. List again — confirms delete, shows only Alice remains
./customer_cli list
```

While running steps 2–8, switch to the **RDS Monitoring** tab in the AWS Console so the interviewer can see `DatabaseConnections` and `WriteIOPS` responding in real time.

---

## Talking Points for a LabKey / AWS-shop Audience

- **"I used RDS instead of running Postgres on EC2"** — shows awareness of managed services, backups, and operational overhead reduction
- **"RDS is not publicly accessible"** — shows you didn't take the easy/insecure path
- **"Security groups reference each other, not IP ranges"** — shows understanding of AWS network security beyond basics
- **"Multi-stage Docker build"** — builder image compiles C++, runtime image has no compiler or build tools; smaller attack surface
- **"Parameterized queries throughout"** — no string-interpolated SQL anywhere in the codebase
- **"I'd add a secrets manager integration next"** — DB password is in an env var; next step would be AWS Secrets Manager with IAM role-based access, eliminating the plaintext credential entirely
- **"The same Docker image runs locally and on EC2"** — only the env vars change between environments; no environment-specific code
