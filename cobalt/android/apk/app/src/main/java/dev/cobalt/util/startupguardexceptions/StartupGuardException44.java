package dev.cobalt.util.startupguardexceptions;

/**
 * Custom runtime exception for startup-related guard failures.
 */
public class StartupGuardException44 extends RuntimeException {

    // Default constructor
    public StartupGuardException44() {
        super();
    }

    // Constructor that accepts a custom error message
    public StartupGuardException44(String message) {
        super(message);
    }

    // Constructor that accepts a message and a cause (for exception chaining)
    public StartupGuardException44(String message, Throwable cause) {
        super(message, cause);
    }

    // Constructor that accepts only a cause
    public StartupGuardException44(Throwable cause) {
        super(cause);
    }
}
