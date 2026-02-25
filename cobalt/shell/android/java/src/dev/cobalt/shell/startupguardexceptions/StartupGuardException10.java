package dev.cobalt.shell.startupguardexceptions;

/**
 * Custom runtime exception for startup-related guard failures.
 */
public class StartupGuardException10 extends RuntimeException {

    // Default constructor
    public StartupGuardException10() {
        super();
    }

    // Constructor that accepts a custom error message
    public StartupGuardException10(String message) {
        super(message);
    }

    // Constructor that accepts a message and a cause (for exception chaining)
    public StartupGuardException10(String message, Throwable cause) {
        super(message, cause);
    }

    // Constructor that accepts only a cause
    public StartupGuardException10(Throwable cause) {
        super(cause);
    }
}
