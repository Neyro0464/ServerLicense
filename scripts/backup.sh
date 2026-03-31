#!/bin/sh

# Set timestamp for the backup filename
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
FILENAME="/backups/backup_${TIMESTAMP}.sql"

echo "Starting backup of database: ${DB_NAME} at ${TIMESTAMP}"

# Run pg_dump
# Environment variables DB_HOST, DB_NAME, DB_USER, and PGPASSWORD are provided by docker-compose
pg_dump -h "${DB_HOST}" -U "${DB_USER}" -d "${DB_NAME}" > "${FILENAME}"

# Check if pg_dump succeeded
if [ $? -eq 0 ]; then
    echo "Backup completed successfully! Saved as: ${FILENAME}"
    # Keep only las t 7 days of backups
    find /backups -type f -name "backup_*.sql" -mtime +7 -exec rm {} \;
    echo "Old backups cleaned up."
else
    echo "Backup FAILED! Check connection settings and database status."
fi
