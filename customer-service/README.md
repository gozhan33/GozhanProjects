# Customer Service — Setup & Deployment Guide

## Prerequisites
- AWS account with $100 credits applied
- Docker installed locally
- A terminal (macOS/Linux) or WSL2 on Windows
- psql installed locally (for schema init via SSH tunnel)

---

## Part 1 — Test locally first (no AWS needed)

```bash
# 1. Build and start both containers
docker-compose up --build

# 2. In a second terminal, build and run the CLI
cd client
git clone --depth 1 https://github.com/yhirose/cpp-httplib
git clone --depth 1 https://github.com/nlohmann/json
g++ -std=c++17 -O2 -I cpp-httplib -I json/include main.cpp \
    -o customer_cli -lssl -lcrypto

export API_URL=http://localhost:8080
./customer_cli list
./customer_cli create "Alice Smith" alice@example.com
./customer_cli get 1
./customer_cli update 1 --name "Alice Cooper"
./customer_cli delete 1
```

If this works, the service is correct. Now deploy to AWS.

---

## Part 2 — AWS Console: Create RDS PostgreSQL

### 2a. Open RDS
1. AWS Console → search "RDS" → click **RDS**
2. Click **Create database**

### 2b. Configure the database
| Setting | Value |
|---|---|
| Creation method | Standard create |
| Engine | PostgreSQL |
| Version | PostgreSQL 15 |
| Template | **Free tier** |
| DB instance identifier | `customer-db` |
| Master username | `csadmin` |
| Master password | (choose a strong password, save it) |
| DB instance class | db.t3.micro (auto-selected by free tier) |
| Storage | 20 GiB gp2 |
| Public access | **No** |
| VPC | Default VPC |
| DB name (under Additional config) | `customerdb` |

3. Click **Create database** — takes ~5 minutes.
4. Once status is **Available**, click the DB → copy the **Endpoint** (looks like `customer-db.xxxx.us-east-1.rds.amazonaws.com`).

---

## Part 3 — AWS Console: Create EC2 Instance

### 3a. Open EC2
1. AWS Console → search "EC2" → click **EC2**
2. Click **Launch instance**

### 3b. Configure the instance
| Setting | Value |
|---|---|
| Name | `customer-service-backend` |
| AMI | Amazon Linux 2023 (free tier eligible) |
| Instance type | t2.micro |
| Key pair | Create new → name it `customer-service-key` → download the .pem file |
| Network | Default VPC |
| Auto-assign public IP | Enable |
| Security group (new) | See below |

### 3c. Security group rules (create new, name it `customer-service-sg`)
| Type | Port | Source |
|---|---|---|
| SSH | 22 | My IP (auto-filled) |
| Custom TCP | 8080 | My IP (for testing) |

> For the API to be publicly accessible, set port 8080 source to `0.0.0.0/0`.
> For private testing, keep it as My IP.

### 3d. User data (Advanced details → User data field)
Paste this to auto-install Docker on first boot:
```bash
#!/bin/bash
dnf update -y
dnf install -y docker
systemctl enable docker
systemctl start docker
usermod -aG docker ec2-user
```

3. Click **Launch instance**. Takes ~1 minute.
4. Copy the **Public IPv4 address** from the instance details.

---

## Part 4 — Allow EC2 to reach RDS (Security Group)

1. Go to **RDS** → click your `customer-db` → **Connectivity & security**
2. Click the VPC security group link → **Edit inbound rules**
3. Add rule:
   | Type | Port | Source |
   |---|---|---|
   | PostgreSQL | 5432 | Custom → select `customer-service-sg` |
4. Save rules.

---

## Part 5 — Initialize the database schema

RDS is not publicly accessible, so use an SSH tunnel through EC2:

```bash
EC2_IP=<your-ec2-public-ip>
RDS_HOST=<your-rds-endpoint>

# Open SSH tunnel: localhost:5433 → RDS:5432
chmod 400 ~/Downloads/customer-service-key.pem
ssh -i ~/Downloads/customer-service-key.pem \
    -L 5433:${RDS_HOST}:5432 \
    ec2-user@${EC2_IP} -N &

# Run the schema
psql -h localhost -p 5433 -U csadmin -d customerdb -f sql/schema.sql
# Enter your DB password when prompted

# Close the tunnel
kill %1
```

---

## Part 6 — Build and deploy the Docker image to EC2

```bash
EC2_IP=<your-ec2-public-ip>

# Build the image locally
docker build -t customer-service:latest ./backend

# Copy the image to EC2 over SSH (no ECR needed — saves cost)
docker save customer-service:latest | gzip | \
  ssh -i ~/Downloads/customer-service-key.pem \
  ec2-user@${EC2_IP} 'gunzip | docker load'

# SSH into EC2 and start the container
ssh -i ~/Downloads/customer-service-key.pem ec2-user@${EC2_IP}

# On the EC2 instance:
docker run -d \
  --name customer-service \
  --restart unless-stopped \
  -p 8080:8080 \
  -e DB_HOST="<your-rds-endpoint>" \
  -e DB_PORT=5432 \
  -e DB_USER=csadmin \
  -e DB_PASS="<your-db-password>" \
  -e DB_NAME=customerdb \
  -e DB_SSLMODE=require \
  customer-service:latest

# Verify
curl http://localhost:8080/health
# Expected: {"status":"ok"}

exit  # back to your laptop
```

---

## Part 7 — Run the CLI client against EC2

```bash
export API_URL=http://<your-ec2-public-ip>:8080

./customer_cli list
./customer_cli create "Alice Smith" alice@example.com
./customer_cli create "Bob Jones"   bob@example.com
./customer_cli list
./customer_cli get 1
./customer_cli update 1 --name "Alice Cooper"
./customer_cli update 2 --email new@example.com
./customer_cli delete 2
./customer_cli list
```

---

## Stopping to avoid charges

**Stop EC2** (keeps data, no compute charge):
- EC2 Console → select instance → Instance state → **Stop**

**Stop RDS** (keeps data, no instance charge):
- RDS Console → select DB → Actions → **Stop temporarily** (stops for 7 days, then auto-restarts — set a reminder)

**Delete everything** (no further charges):
- RDS → Delete (uncheck final snapshot for test data)
- EC2 → Terminate instance

> Set a billing alert: AWS Console → Billing → Budgets → Create budget → $5 threshold.
> You'll get an email before anything meaningful is charged.

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| `Connection failed` from CLI | Port 8080 not open | Check EC2 security group inbound rules |
| `DB connection FAILED` at startup | Wrong RDS endpoint or password | Check env vars passed to `docker run` |
| `SSL connection required` | sslmode not set | `db.h` already sets `sslmode=require` — ensure RDS has SSL enabled (default) |
| Container exits immediately | Check logs: `docker logs customer-service` | Usually a missing env var |
| Schema init fails | Tunnel not open / wrong port | Verify tunnel with `psql -h localhost -p 5433 -U csadmin -d customerdb -c '\dt'` |
