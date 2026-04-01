#!/bin/sh

# Timestamp for the backup filename
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
FILENAME="/backups/backup_${TIMESTAMP}.sql"

echo "Starting backup of database: ${PGDATABASE} at ${TIMESTAMP}"

# Run pg_dump using standard libpq environment variables:
# PGHOST, PGDATABASE, PGUSER, PGPASSWORD — set by docker-compose
pg_dump -h "${PGHOST}" -U "${PGUSER}" -d "${PGDATABASE}" > "${FILENAME}"

# Check if pg_dump succeeded
if [ $? -eq 0 ]; then
    echo "Backup completed successfully! Saved as: ${FILENAME}"
    # Keep only last 7 days of backups
    find /backups -type f -name "backup_*.sql" -mtime +7 -exec rm {} \;
    echo "Old backups cleaned up."
else
    echo "Backup FAILED! Check connection settings and database status."
fi
