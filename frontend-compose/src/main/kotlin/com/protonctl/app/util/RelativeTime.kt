package com.protonctl.app.util

import java.time.Instant
import java.time.ZoneId
import java.time.format.DateTimeFormatter
import java.time.temporal.ChronoUnit

private val EXACT_DATE_FORMATTER = DateTimeFormatter.ofPattern("MMM d, yyyy")

private fun plural(count: Long, unit: String) = "$count $unit${if (count == 1L) "" else "s"} ago"

/**
 * Formats an ISO-8601 UTC timestamp (native install markers, GitHub's
 * `published_at`, etc.) the same way everywhere in the UI: seconds/
 * minutes/hours/days/months ago while it's still this calendar year,
 * "This year" as a catch-all for anything in that range the buckets above
 * don't cleanly cover, and the exact date once it's a past year.
 *
 * Returns the raw string unchanged if it isn't a parseable timestamp
 * (e.g. empty, for installs that predate the marker file this reads from).
 */
fun formatRelativeDate(isoTimestamp: String, now: Instant = Instant.now()): String {
    if (isoTimestamp.isBlank()) return ""

    val instant = try {
        Instant.parse(isoTimestamp)
    } catch (_: Exception) {
        return isoTimestamp
    }

    val zone = ZoneId.systemDefault()
    val instantZoned = instant.atZone(zone)
    val nowZoned = now.atZone(zone)

    if (instantZoned.year != nowZoned.year) {
        return EXACT_DATE_FORMATTER.format(instantZoned)
    }

    val seconds = ChronoUnit.SECONDS.between(instant, now).coerceAtLeast(0)
    val minutes = ChronoUnit.MINUTES.between(instant, now)
    val hours = ChronoUnit.HOURS.between(instant, now)
    val days = ChronoUnit.DAYS.between(instant, now)
    val months = ChronoUnit.MONTHS.between(instantZoned, nowZoned)

    return when {
        seconds < 60 -> plural(seconds, "second")
        minutes < 60 -> plural(minutes, "minute")
        hours < 24 -> plural(hours, "hour")
        days < 30 -> plural(days, "day")
        months in 1..11 -> plural(months, "month")
        else -> "This year"
    }
}
