package com.todolist.mobile.core

import org.junit.Assert.assertEquals
import org.junit.Test

class MergePolicyTest {

    private val t = 1_700_000_000_000L // 基准 ms
    private val devA = "00000000-0000-4000-8000-00000000000a"
    private val devB = "00000000-0000-4000-8000-00000000000b"

    @Test
    fun newerClientWins() {
        assertEquals(MergePolicy.Verdict.ACCEPT, MergePolicy.decide(t + 5000, devA, t, devB))
    }

    @Test
    fun olderClientLoses() {
        assertEquals(MergePolicy.Verdict.CONFLICT, MergePolicy.decide(t - 5000, devA, t, devB))
    }

    @Test
    fun equalTimeTieBreakByDevice() {
        assertEquals(MergePolicy.Verdict.CONFLICT, MergePolicy.decide(t, devA, t, devB))
        assertEquals(MergePolicy.Verdict.ACCEPT, MergePolicy.decide(t, devB, t, devA))
    }

    @Test
    fun equalTimeSameDeviceAccepts() {
        assertEquals(MergePolicy.Verdict.ACCEPT, MergePolicy.decide(t, devA, t, devA))
    }

    @Test
    fun nullServerDeviceAccepts() {
        assertEquals(MergePolicy.Verdict.ACCEPT, MergePolicy.decide(t, devA, t, null))
    }
}
