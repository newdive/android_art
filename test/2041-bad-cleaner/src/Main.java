/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import dalvik.system.VMRuntime;
import sun.misc.Cleaner;
import java.util.concurrent.CountDownLatch;
import static java.util.concurrent.TimeUnit.MINUTES;

/**
 * Test a class with a bad finalizer.
 *
 * This test is inherently flaky. It assumes that the system will schedule the finalizer daemon
 * and finalizer watchdog daemon enough to reach the timeout and throwing the fatal exception.
 */
public class Main {
    public static void main(String[] args) throws Exception {
        CountDownLatch finalizerWait = new CountDownLatch(1);

        // A separate method to ensure no dex register keeps the object alive.
        createBadCleanable(finalizerWait);

        // Should have at least two iterations to trigger finalization, but just to make sure run
        // some more.
        for (int i = 0; i < 5; i++) {
            Runtime.getRuntime().gc();
        }

        // Now wait for the finalizer to start running. Give it a minute.
        finalizerWait.await(1, MINUTES);

        // Now fall asleep with a timeout. The timeout is large enough that we expect the
        // finalizer daemon to have killed the process before the deadline elapses.
        // The timeout is also large enough to cover the extra 5 seconds we wait
        // to dump threads, plus potentially substantial gcstress overhead.
        // The RQ timeout is currently effectively 5 * the finalizer timeout.
        // Note: the timeout is here (instead of an infinite sleep) to protect the test
        //       environment (e.g., in case this is run without a timeout wrapper).
        final long timeout = 100 * 1000 + 5 * VMRuntime.getRuntime().getFinalizerTimeoutMs();
        long remainingWait = timeout;
        final long waitStart = System.currentTimeMillis();
        while (remainingWait > 0) {
            synchronized (args) {  // Just use an already existing object for simplicity...
                try {
                    args.wait(remainingWait);
                } catch (Exception e) {
                    System.out.println("UNEXPECTED EXCEPTION");
                }
            }
            remainingWait = timeout - (System.currentTimeMillis() - waitStart);
        }

        // We should not get here.
        System.out.println("UNREACHABLE");
        System.exit(0);
    }

    private static void createBadCleanable(CountDownLatch finalizerWait) {
        Object badCleanable = new Object();
        Runnable badRunnable = new Runnable() {
            public void run () {
                finalizerWait.countDown();

                System.out.println("Cleaner started and sleeping briefly...");

                snooze(2000);
                System.out.println("Cleaner done snoozing.");

                System.out.println("Cleaner sleeping forever now.");
                while (true) {
                    snooze(500000);
                }
            }
        };
        final Cleaner badCleaner = Cleaner.create(badCleanable, badRunnable);

        System.out.println("About to null reference.");
        badCleanable = null;  // Not that this would make a difference, could be eliminated earlier.
    }

    public static void snooze(int ms) {
        try {
            Thread.sleep(ms);
        } catch (InterruptedException ie) {
        }
    }

}
