#include "timer1.h"
#include "events.h"

void continuous2idle()
{
    if (sys.status == STATUS::CONTINUOUS_ACQ)
    {
        if (sys.n_frames > 0) // There is a frame limit
        {
            if (sys.n_acquired_frames >= sys.n_frames) // We acquired enough frames!
            {
                sys.n_acquired_frames = 0;
                sys.status = STATUS::IDLE; // shutdown everything after the next cycle
                Serial.println("DONE");
            }
        }
    }
}

void prepare_next_frame()
{
    continuous2idle();

    //   normal2skip();
    //   skip2normal();
    //   alex2idle();
    //   alex2skip();
    //   skip2alex();
}

// -----------------------------------------------------

void setup_timer1()
{
    // WGM mode 14 (fast PWM with ICR1 max)
    TCCR1A = bit(WGM11);
    TCCR1B = bit(WGM12) | bit(WGM13) | TCCR1B_prescaler_bits;

    // Default settings
    set_interframe_duration_us(10000);
    set_matchA_us(LASER_SHUTTER_DELAY);
    set_matchB_us(LASER_SHUTTER_DELAY + 1000);

    // Enable interrupts for overflow, match A, and match B
    TIMSK1 = bit(TOIE1) | bit(OCIE1A) | bit(OCIE1B);
}

void reset_timer1()
{
    uint8_t old_SREG = SREG;
    cli();
    sys.time += TCNT1;
    TCNT1 = 0;
    SREG = old_SREG;
}

void set_timer1_values()
{
    // Set timer period to requested value, and at least N counts
    ICR1 = max(us2cts(sys.interframe_time_us), 5);

    // Set delay between shutter and camera
    OCR1A = us2cts(LASER_SHUTTER_DELAY);
    OCR1A = min(OCR1A, ICR1 - 2); // Sanity check: make sure OCR1A is lower than ICR1

    // 12.5% duty cycle, at least 1 count, at most 10ms
    OCR1B = OCR1A + max(min(ICR1 >> 3, us2cts(10000)), 1);
    OCR1B = min(OCR1B, ICR1 - 1); // Sanity check: make sure OCR1B is lower than ICR1

    // Clear interrupt flags
    TIFR1 = 0xFF;

    // Enable interrupts for overflow, match A, and match B
    TIMSK1 = bit(TOIE1) | bit(OCIE1A) | bit(OCIE1B);

    // Reset the timer and make sure it is not paused
    TCNT1 = ICR1 - 2;
    GTCCR = 0;
}

void set_interframe_duration_us(uint32_t us)
{
    uint32_t cts = us2cts(us);

    t1.cycle = 0;
    t1.N_OVF_cycles = 1;
    // We divide required #counts in half until if fits into 16bit
    while (cts >= UINT16_MAX)
    {
        cts >>= 1;             // divide in half
        t1.N_OVF_cycles <<= 1; // double number of cycles
    }

    ICR1 = cts - 1;
}

void set_matchA_us(uint32_t us)
{
    uint32_t cts = us2cts(us);

    t1.N_matchA_cycles = cts / (ICR1 + 1);

    OCR1A = cts % ICR1;
}

void set_matchB_us(uint32_t us)
{
    uint32_t cts = us2cts(us);

    t1.N_matchB_cycles = cts / (ICR1 + 1);

    OCR1B = cts % ICR1;
}

// --------------------------------------------------------------

void T1_OVF_event()
{
    if (sys.status == STATUS::CONTINUOUS_ACQ)
    {
        write_shutters(sys.shutter_active);
    }

    if (sys.status == STATUS::IDLE)
    {
        write_shutters(sys.shutter_idle);
    }
}

void T1_COMPA_event()
{
    if (sys.status == STATUS::CONTINUOUS_ACQ)
    {
        camera_pin_up();
        sys.n_acquired_frames++;
    }
}

void T1_COMPB_event()
{
    camera_pin_down();

    // Decide what to do during the next timer cycle.
    prepare_next_frame();
}

// ---------------------------

// interrupt 13, first event during the frame
ISR(TIMER1_OVF_vect)
{
    // Update system time
    sys.time += cts2us(ICR1);

    t1.cycle++;
    if (t1.cycle >= t1.N_OVF_cycles)
    {
        // Time to call interrupt handler
        T1_OVF_event();
        t1.cycle = 0;
    }
}

// interrupt 11, second event during the frame
ISR(TIMER1_COMPA_vect)
{
    if (t1.cycle == t1.N_matchA_cycles)
    {
        T1_COMPA_event();
    }
}

// interrupt 12, third event during the frame
ISR(TIMER1_COMPB_vect)
{
    if (t1.cycle == t1.N_matchB_cycles)
    {
        T1_COMPB_event();
    }
}